#ifndef USBEXAMPLE_H
#define USBEXAMPLE_H

#include <QObject>

#include <QMutex>
#include <QTimer>
#include "AVThread.h"

class UsbExample;
class QUsbEndpoint;
class QUsbDevice;

class UsbExampleTask : public Task{
public :
    ~UsbExampleTask(){}
    enum UsbExampleTaskCommand{
        UsbExampleTaskCommand_Read,
        UsbExampleTaskCommand_ReadyRead,

    };
    UsbExampleTask(UsbExample *codec,UsbExampleTaskCommand command,double param = 0,QString param2 = ""):
        mCodec(codec),command(command),param(param),param2(param2){}
protected :
    /** 现程实现 */
    virtual void run();
private :
    UsbExample *mCodec;
    UsbExampleTaskCommand command;
    double param;
    QString param2;
};

class UsbExample : public QObject
{
    Q_OBJECT
    friend class UsbExampleTask;
public:

    explicit UsbExample(QObject *parent = Q_NULLPTR);
    ~UsbExample(void);
    void setupDevice(void);
    bool openDevice(void);
    void closeDevice(void);
    bool openHandle(void);
    void readUsb();
    void closeHandle(void);
    void read(QByteArray *buf);
    void write(QByteArray *buf);

    void setConfig();
    void getConfig();
    void setOSD(bool state = true);

    void addReadTask();
    void addReadyReadTask();

    void ProcessVideoRead(unsigned char* buf,int len);
public slots:
    void onReadyRead(void);
    void onWriteComplete(qint64 bytes);

    void onConfigReadyRead(void);

private:
    void configRead(QByteArray *buf);
    void configWrite(QByteArray *buf);
signals:
    void sendConfigData(QByteArray);

private:


private:
    QUsbDevice *m_usb_dev;

    QUsbEndpoint *m_read_ep_1;
    QUsbEndpoint *m_read_ep_2;
    QUsbEndpoint *m_write_ep_1;
    QUsbEndpoint *m_write_ep_2;

    QByteArray m_send, m_recv;
    QMutex m_recvMutex;

    AVThread mReadUsbThread;
    AVThread mReadyReadThread;

    QTimer* _readConfigTimer = nullptr;
};

extern UsbExample*    getUsbExample();

#endif // USBEXAMPLE_H
