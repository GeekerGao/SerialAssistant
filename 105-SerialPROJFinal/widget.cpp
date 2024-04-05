#include "widget.h"
#include "ui_widget.h"

#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    this->setLayout(ui->gridLayoutGlobal);
    //控制参数初始化
    writeCntTotal = 0;
    readCntTotal = 0;
    serialStatus = false;
    buttonIndex = 0;
    //控件初始化
    ui->btnSendText->setEnabled(false);
    ui->checkBoxSendInTime->setEnabled(false);
    ui->checkBoxSendNewLine->setEnabled(false);
    ui->checkBoxHexSend->setEnabled(false);
    //在窗口加入一个串口控制对象
    serialPort = new QSerialPort(this);

    //定义一个定时器，每100ms刷新系统时间
    QTimer *getSysTimeTimer = new QTimer(this);
    connect(getSysTimeTimer,SIGNAL(timeout()),this,SLOT(time_reFlash()));
    getSysTimeTimer->start(100);
    //这个定时器用于定时发送
    timer = new QTimer(this);
    buttonConTimer = new QTimer(this);
    connect(buttonConTimer,&QTimer::timeout,this,&Widget::buttons_handler);
    connect(serialPort,&QSerialPort::readyRead,this,&Widget::on_SerialData_readyToRead);
    connect(timer,&QTimer::timeout,[=](){
        on_btnSendText_clicked();
    });
    connect(ui->comboBox_serialnum,&MyComboBox::refresh,this,&Widget::refreshSerialName);

    ui->comboBox_boautrate->setCurrentIndex(6);
    ui->comboBox_databit->setCurrentIndex(3);

    refreshSerialName();

    ui->labelSendStatus->setText(ui->comboBox_serialnum->currentText()+" Not Open!");


    for(int i=1;i<=9;i++){
        QString btnName = QString("pushButton_%1").arg(i);
        QPushButton *btn = findChild<QPushButton *>(btnName);
        if(btn){
            btn->setProperty("buttonId",i);
            buttons.append(btn);
            connect(btn,SIGNAL(clicked()),this,SLOT(on_command_button_clicked()));
        }

        QString lineEditName = QString("lineEdit_%1").arg(i);
        QLineEdit *lineEdit = findChild<QLineEdit *>(lineEditName);
        lineEdits.append(lineEdit);

        QString checkBoxName = QString("checkBox_%1").arg(i);
        QCheckBox *checkBox = findChild<QCheckBox *>(checkBoxName);
        checkBoxs.append(checkBox);
    }
}

Widget::~Widget()
{
    delete ui;
}

//打开/关闭串口（作废）
void Widget::on_btnCloseOpenSerial_clicked()
{

}

//发送数据
void Widget::on_btnSendText_clicked()
{
    int writeCnt = 0;
    //读取内容
    const char* sendData = ui->lineEditSendText->text().toLocal8Bit().constData();
    //判断是否16进制发送，如果是的话
    if(ui->checkBoxHexSend->isChecked()){
        QString tmp = ui->lineEditSendText->text();
        //判断是否是偶数位
        QByteArray tmpArray = tmp.toLocal8Bit();
        if(tmpArray.size() %2 !=0){
            ui->labelSendStatus->setText("Error Input!");
            return;
        }
        //判断是否符合十六进制表达
        for(char c : tmpArray){
            if(!std::isxdigit(c)){
                ui->labelSendStatus->setText("Error Input!");
                return;
            }
        }
        //判断发送新行是否被勾选
        if(ui->checkBoxSendNewLine->isChecked()){
            tmpArray.append("\r\n");
        }
        //转换成十六进制发送，用户输入1，变成1，拒绝变成字符1
        QByteArray arraySend = QByteArray::fromHex(tmpArray);
        writeCnt = serialPort->write(arraySend);
    }else{
        //如果不是16进制发送
        if(ui->checkBoxSendNewLine->isChecked()){
            QByteArray arraySendData(sendData,strlen(sendData));
            arraySendData.append("\r\n");
            writeCnt = serialPort->write(arraySendData);
        }else
            writeCnt = serialPort->write(sendData);
    }
    //发送失败
    if(writeCnt == -1){
        ui->labelSendStatus->setText("Send Error!");
    }
    //发送成功
    else{
        writeCntTotal += writeCnt;
        ui->labelSendStatus->setText("Send OK!");
        ui->labelSendCnt->setText("Sent:"+QString::number(writeCntTotal));//界面下方显示发送数据大小
        if(strcmp(sendData,sendBak.toStdString().c_str()) != 0){
            ui->textEditRecord->append(sendData);
            sendBak = QString::fromUtf8(sendData);
        }
    }
}
//接收数据
void Widget::on_SerialData_readyToRead()
{
    QString revMessage = serialPort->readAll();
    //判断数据是否为空
    if(revMessage != NULL){
        //自动换行是否被勾选
        if(ui->checkBoxLine->isChecked()) revMessage.append("\r\n");
        //判断Hex显示是否被勾选
        if(ui->checkBoxDisplay->isChecked()){ //Hex被勾选
            QByteArray tmpHexString = revMessage.toUtf8().toHex().toUpper();
            //原来控件上的内容，Hex
            QString tmpStringHex = ui->textEditRev->toPlainText();//因为勾选了，读出来的就是Hex
            tmpHexString = tmpStringHex.toUtf8() + tmpHexString; //把读出的旧的Hex和新收到的数据转成Hex进行拼接
            ui->textEditRev->setText(QString::fromUtf8(tmpHexString));
        }else{  //Hex显示未被勾选
            if(ui->checkBoxRevTime->checkState()== Qt::Unchecked){  //判断接收时间按钮是否被按下
                ui->textEditRev->insertPlainText(revMessage);
            }
            else if(ui->checkBoxRevTime->checkState()== Qt::Checked){
                getSysTime();
                ui->textEditRev->insertPlainText("【"+myTime+"】 "+revMessage);
            }
        }
        readCntTotal += revMessage.size();//计算收到的数据大小
        ui->labelRevCnt->setText("Received:"+QString::number(readCntTotal));//界面下方显示接收数据大小

        ui->textEditRev->moveCursor(QTextCursor::End);
        ui->textEditRev->ensureCursorVisible();
    }
}
//打开/关闭串口，带checked
void Widget::on_btnCloseOpenSerial_clicked(bool checked)
{
    if(checked){
        //打开串口后，设置各个控件的失能，设置成不可配置状态
        //1.选择端口号
        serialPort->setPortName(ui->comboBox_serialnum->currentText());
        //2.配置波特率
        serialPort->setBaudRate(ui->comboBox_boautrate->currentText().toInt());
        //3.配置数据位
        serialPort->setDataBits(QSerialPort :: DataBits(
                                    ui->comboBox_databit->currentText().toUInt()));
        //4.配置校验位
        switch(ui->comboBox_jiaoyan->currentIndex()){
        case 0:
            serialPort->setParity(QSerialPort :: NoParity);
            break;
        case 1:
            serialPort->setParity(QSerialPort :: EvenParity);
            break;
        case 2:
            serialPort->setParity(QSerialPort :: MarkParity);
            break;
        case 3:
            serialPort->setParity(QSerialPort :: OddParity);
            break;
        case 4:
            serialPort->setParity(QSerialPort :: SpaceParity);
            break;
        default:
            serialPort->setParity(QSerialPort :: UnknownParity);
            break;
        }
        //5.配置停止位
        serialPort->setStopBits(QSerialPort :: StopBits(
                                    ui->comboBox_stopbit->currentText().toUInt()));
        //6.流控
        if(ui->comboBox_fileCon->currentText() == "None")
            serialPort->setFlowControl(QSerialPort :: NoFlowControl);
        //7.打开串口
        if(serialPort->open(QIODevice::ReadWrite)){
            ui->comboBox_databit->setEnabled(false);
            ui->comboBox_fileCon->setEnabled(false);
            ui->comboBox_jiaoyan->setEnabled(false);
            ui->comboBox_stopbit->setEnabled(false);
            ui->comboBox_boautrate->setEnabled(false);
            ui->comboBox_serialnum->setEnabled(false);
            ui->btnCloseOpenSerial->setText("关闭串口");
            ui->btnSendText->setEnabled(true);
            ui->checkBoxSendInTime->setEnabled(true);
            ui->checkBoxSendNewLine->setEnabled(true);
            ui->checkBoxHexSend->setEnabled(true);
            ui->labelSendStatus->setText(ui->comboBox_serialnum->currentText()+" isOpen!");
        }else{
            QMessageBox msgBox;
            msgBox.setWindowTitle("打开串口错误");
            msgBox.setText("打开失败，串口可能被占用/拔出!");
            msgBox.exec();
        }
    }else{
        //关闭串口后，设置各个控件的使能，恢复可配置状态
        serialPort->close();
        ui->btnCloseOpenSerial->setText("打开串口");
        ui->comboBox_databit->setEnabled(true);
        ui->comboBox_fileCon->setEnabled(true);
        ui->comboBox_jiaoyan->setEnabled(true);
        ui->comboBox_stopbit->setEnabled(true);
        ui->comboBox_boautrate->setEnabled(true);
        ui->comboBox_serialnum->setEnabled(true);
        ui->btnSendText->setEnabled(false);
        ui->checkBoxSendInTime->setEnabled(false);
        ui->checkBoxSendInTime->setCheckState(Qt::Unchecked);
        //关闭定时器
        timer->stop();
        ui->lineEditTimeEach->setEnabled(true);
        ui->lineEditSendText->setEnabled(true);
        ui->checkBoxSendNewLine->setEnabled(false);
        ui->checkBoxHexSend->setEnabled(false);
        ui->labelSendStatus->setText(ui->comboBox_serialnum->currentText()+" isClose!");
    }
}
//是否定时发送
void Widget::on_checkBoxSendInTime_clicked(bool checked)
{
    if(checked){
        ui->lineEditTimeEach->setEnabled(false);
        ui->lineEditSendText->setEnabled(false);
        //定时器开始
        timer->start(ui->lineEditTimeEach->text().toInt());
    }else{
        //定时器关闭
        timer->stop();
        ui->lineEditTimeEach->setEnabled(true);
        ui->lineEditSendText->setEnabled(true);
    }
}
//清除接收
void Widget::on_btnRevClear_clicked()
{
    ui->textEditRev->setText("");
}
//保存接收记录到文件
void Widget::on_btnRevSave_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                                                    "D:/Study/serialData.txt",
                                                    tr("Text (*.txt)"));
    if(fileName != NULL){
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        QTextStream out(&file);
        out << ui->textEditRev->toPlainText();
        file.close();
    }
}
//刷新时间显示
void Widget::time_reFlash()
{
    getSysTime();
    ui->labelCurrentTime->setText(myTime);
}
//获取系统时间
void Widget::getSysTime()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    //获取日期
    QDate data = currentTime.date();
    int year = data.year();
    int month = data.month();
    int day = data.day();
    //获取时间
    QTime time = currentTime.time();
    int hour = time.hour();
    int minute = time.minute();
    int second = time.second();
    //掌握C++中字符串拼接方法，与C语言不同
    myTime = QString("%1-%2-%3 %4:%5:%6")
            .arg(year,2,10,QChar('0'))
            .arg(month,2,10,QChar('0'))
            .arg(day,2,10,QChar('0'))
            .arg(hour,2,10,QChar('0'))
            .arg(minute,2,10,QChar('0'))
            .arg(second,2,10,QChar('0'));
}
//是否Hex显示
void Widget::on_checkBoxDisplay_clicked(bool checked)
{
    if(checked){
        //1.读取textedit内容(a)
        QString tmp = ui->textEditRev->toPlainText();
        //2.QString的'a'转换为QByteArray的'a'
        QByteArray qtmp = tmp.toUtf8();
        //3.QByteArray的'a'转换为QByteArray的61
        qtmp = qtmp.toHex();
        //4.QByteArray的61转换为QString的61，显示
        QString lastShow;
        tmp = QString::fromUtf8(qtmp);
        for(int i=0;i<tmp.size();i+=2){
            lastShow += tmp.mid(i,2) + " ";
        }
        ui->textEditRev->setText(lastShow.toUpper());
    }else{
        //1.读取textedit内容，hex(61)
        QString tmpHexString = ui->textEditRev->toPlainText();
        //2.QString的61转换为QByteArray的61
        QByteArray tmpHexQByteArray = tmpHexString.toUtf8();
        //3.QByteArray的61转换为QByteArray的'a'
        QByteArray tmpQByteString = QByteArray::fromHex(tmpHexQByteArray);
        //4.把QByteArray的'a'转换为QString的'a'，显示
        ui->textEditRev->setText(QString::fromUtf8(tmpQByteString));
    }
    ui->textEditRev->moveCursor(QTextCursor::End);
    ui->textEditRev->ensureCursorVisible();
}

void Widget::on_btnHideTable_clicked(bool checked)
{
    if(checked){
        ui->btnHideTable->setText("拓展面板");
        ui->groupBoxTexts->hide();
    }else{
        ui->btnHideTable->setText("隐藏面板");
        ui->groupBoxTexts->show();
    }
}

void Widget::on_btnHideHistory_clicked(bool checked)
{
    if(checked){
        ui->btnHideHistory->setText("显示历史");
        ui->groupBoxRecord->hide();
    }else{
        ui->btnHideHistory->setText("隐藏历史");
        ui->groupBoxRecord->show();
    }
}

void Widget::refreshSerialName()
{
    ui->comboBox_serialnum->clear();
    //获取端口号
    QList<QSerialPortInfo>serialList =  QSerialPortInfo::availablePorts();
    for(QSerialPortInfo serialInfo : serialList){
        ui->comboBox_serialnum->addItem(serialInfo.portName());
    }
    ui->labelSendStatus->setText("Com Refreshed!");
}

void Widget::on_command_button_clicked()
{
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if(btn){
        int num = btn->property("buttonId").toInt();
        QString lineEditName = QString("lineEdit_%1").arg(num);
        QLineEdit *lineEdit = findChild<QLineEdit *>(lineEditName);
        if(lineEdit){
            if(lineEdit->text().size() <= 0){
                return;
            }
            ui->lineEditSendText->setText(lineEdit->text());
        }
        QString checkBoxName = QString("checkBox_%1").arg(num);
        QCheckBox *checkBox = findChild<QCheckBox *>(checkBoxName);
        if(checkBox)
            ui->checkBoxHexSend->setChecked(checkBox->isChecked());
        on_btnSendText_clicked();
    }
}

void Widget::buttons_handler()
{
    if(buttonIndex < buttons.size()){
        QPushButton *btnTmp = buttons[buttonIndex];
        emit btnTmp->clicked();
        buttonIndex++;
    }else{
        buttonIndex = 0;
    }
}

void Widget::on_checkBox_send_clicked(bool checked)
{
    if(checked){
        ui->spinBox->setEnabled(false);
        buttonConTimer->start(ui->spinBox->text().toUInt());
    }else{
        ui->spinBox->setEnabled(true);
        buttonConTimer->stop();
    }
}

void Widget::on_btnInit_clicked()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("提示");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText("重置列表不可逆，确认是否重置？");
    //msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    QPushButton *yesButton = msgBox.addButton("是",QMessageBox::YesRole);
    QPushButton *noButton = msgBox.addButton("否",QMessageBox::NoRole);
    msgBox.exec();
    if(msgBox.clickedButton() == yesButton){
        for(int i=0;i<lineEdits.size();i++){
            //遍历lineEdit并清空内容,
            lineEdits[i]->clear();
            //遍历checkBox并取消勾选
            checkBoxs[i]->setChecked(false);
        }
    }
    if(msgBox.clickedButton() == noButton){

    }
}

void Widget::on_btnSave_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("保存文件"),
                                                    "D:/Study",
                                                    tr("文件类型 (*.txt)"));
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&file);
    for(int i=0;i<lineEdits.size();i++){
        out << checkBoxs[i]->isChecked() << "," << lineEdits[i]->text() << "\n";
    }
    file.close();
}

void Widget::on_btnLoad_clicked()
{
    int i = 0;
    QString fileName = QFileDialog::getOpenFileName(this, tr("打开文件"),
                                                    "D:/Study",
                                                    tr("文件类型 (*.txt)"));
    if(fileName != NULL){
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QTextStream in(&file);
        while(!in.atEnd() && i <= 9){
            QString line = in.readLine();
            QStringList parts = line.split("|");
            if(parts.count() == 2){
                checkBoxs[i]->setChecked(parts[0].toUInt());
                lineEdits[i]->setText(parts[1]);
            }
            i++;
        }
    }
}
