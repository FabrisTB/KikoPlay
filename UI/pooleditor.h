#ifndef POOLEDITOR_H
#define POOLEDITOR_H

#include <QDialog>
#include <QFrame>
#include "Play/Danmu/danmupool.h"
#include "framelessdialog.h"
class QVBoxLayout;
class QSpacerItem;
class PoolItem : public QFrame
{
    Q_OBJECT
public:
    explicit PoolItem(const DanmuSource *sourceInfo, QWidget *parent=nullptr);
};

class PoolEditor : public CFramelessDialog
{
    Q_OBJECT
    friend class PoolItem;
public:
    explicit PoolEditor(QWidget *parent = nullptr);
    void refreshItems();
private:
    QVBoxLayout *poolItemVLayout;
    Pool *curPool;
    QVector<PoolItem *> poolItems;
};

#endif // POOLEDITOR_H
