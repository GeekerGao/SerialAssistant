#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QSerialPortInfo>
#include <QDebug>
#include <QSerialPort>
#include <QTimer>
#include <QPushButton>
#include <QCheckBox>
#include "mycombobox.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    //此函数作废
    void on_btnCloseOpenSerial_clicked();
    //发送槽函数
    void on_btnSendText_clicked();
    //接收槽函数
    void on_SerialData_readyToRead();
    //打开/关闭串口，按键带checked
    void on_btnCloseOpenSerial_clicked(bool checked);
    //是否定时发送
    void on_checkBoxSendInTime_clicked(bool checked);
    //清空接收区
    void on_btnRevClear_clicked();
    //保存接收记录到文件
    void on_btnRevSave_clicked();
    //刷新时间显示
    void time_reFlash();
    //是否16进制显示
    void on_checkBoxDisplay_clicked(bool checked);

    void on_btnHideTable_clicked(bool checked);

    void on_btnHideHistory_clicked(bool checked);

    void refreshSerialName();

    void on_command_button_clicked();

    void on_checkBox_send_clicked(bool checked);

    void buttons_handler();

    void on_btnInit_clicked();

    void on_btnSave_clicked();

    void on_btnLoad_clicked();

private:
    Ui::Widget *ui;
    QSerialPort *serialPort;
    int writeCntTotal;
    int readCntTotal;
    QString sendBak;
    bool serialStatus;
    QTimer *timer;
    QString myTime;
    QTimer *buttonConTimer;

    void getSysTime();
    int buttonIndex;
    QList<QPushButton *> buttons;
    QList<QLineEdit *> lineEdits;
    QList<QCheckBox *> checkBoxs;
};
#endif // WIDGET_H
