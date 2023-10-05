#include "notifier.h"
#include <QThread>
Notifier::Notifier(QObject *parent) : QObject(parent)
{

}

void Notifier::cancelCallBack(NotifyInterface *interface)
{
    Notifier *notifier = getNotifier();
    emit notifier->cancelTrigger(interface->getType());
}

void Notifier::addNotify(Notifier::NotifyType nType, NotifyInterface *notify)
{
    notifyHash[nType].append(notify);
    notify->setType(notify->getType() | nType);
    notify->setCancelCallBack(cancelCallBack);
}

void Notifier::showMessage(NotifyType nType, const QString &content, int flag, const QVariant &extra)
{
    if(!notifyHash.contains(nType)) return;
    auto &notifyList = notifyHash[nType];
    Qt::ConnectionType connType = Qt::AutoConnection;
    if(QThread::currentThread()!=this->thread())
    {
        if(flag & NM_DARKNESS_BACK)
        {
            connType = Qt::BlockingQueuedConnection;
        }
    }
    for(auto n: notifyList)
    {
        QMetaObject::invokeMethod(this, [=](){
            n->showMessage(content, flag, extra);
        }, connType);
    }
}

QVariant Notifier::showDialog(Notifier::NotifyType nType, const QVariant &inputs)
{
    if(!notifyHash.contains(nType)) return QVariant();
    auto &notifier = notifyHash[nType].first();
    if(QThread::currentThread()==this->thread())
        return notifier->showDialog(inputs);
    QVariant ret;
    QMetaObject::invokeMethod(this, [&ret, notifier, inputs](){
        ret = notifier->showDialog(inputs);
    }, Qt::BlockingQueuedConnection);
    return ret;
}

Notifier *Notifier::getNotifier()
{
    static Notifier notifier;
    return &notifier;
}
