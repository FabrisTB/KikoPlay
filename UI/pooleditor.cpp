#include "pooleditor.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QAction>
#include <QApplication>
#include "globalobjects.h"
#include "Play/Danmu/Manager/pool.h"
#include "Play/Video/mpvplayer.h"
#include "timelineedit.h"
#include "Common/notifier.h"
#include "danmuview.h"
namespace
{
    QVector<QPair<int,int> > timelineClipBoard;
    QVector<PoolItem *> *items;
    PoolEditor *editor;
    class PoolSignalBlock
    {
    public:
        PoolSignalBlock()
        {
            Pool *curPool = GlobalObjects::danmuPool->getPool();
            if (curPool && editor)
            {
                curPool->disconnect(editor);
            }
        }
        ~PoolSignalBlock()
        {
            if (editor)
            {
                QObject::connect(GlobalObjects::danmuPool->getPool(), &Pool::poolChanged, editor, &PoolEditor::refreshItems);
            }
        }
    };
}
PoolEditor::PoolEditor(QWidget *parent) : CFramelessDialog(tr("Edit Pool"), parent, false, true, false), curPool(nullptr)
{
    items=&poolItems;
    editor=this;
    QWidget *contentWidget=new QWidget(this);

    contentWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    QScrollArea *contentScrollArea=new QScrollArea(this);
    contentScrollArea->setObjectName(QStringLiteral("PoolItemContainer"));
    contentScrollArea->setWidget(contentWidget);
    contentScrollArea->setWidgetResizable(true);
    contentScrollArea->setAlignment(Qt::AlignCenter);

    poolItemVLayout=new QVBoxLayout(contentWidget);
    poolItemVLayout->addStretch(1);
    refreshItems();

    QObject::connect(GlobalObjects::danmuPool, &DanmuPool::poolIdChanged, this, &PoolEditor::refreshItems);

    QPushButton *shareCodeButton=new QPushButton(tr("Share Pool Code"),this);
    QObject::connect(shareCodeButton,&QPushButton::clicked,this,[this](){
        QString code(GlobalObjects::danmuPool->getPool()->getPoolCode());
        if(code.isEmpty())
        {
            this->showMessage(tr("No Danmu Source to Share"), NM_ERROR | NM_HIDE);
        }
        else
        {
            QClipboard *cb = QApplication::clipboard();
            cb->setText("kikoplay:pool="+code);
            this->showMessage(tr("Pool Code has been Copied to Clipboard"));
        }

    });

    QPushButton *addCodeButton=new QPushButton(tr("Add Pool Code"),this);
    QObject::connect(addCodeButton,&QPushButton::clicked,this,[this](){
        QClipboard *cb = QApplication::clipboard();
        QString code(cb->text());
        if(code.isEmpty() ||
                (!code.startsWith("kikoplay:pool=") &&
                !code.startsWith("kikoplay:anime="))) return;
        PoolSignalBlock block;
        bool ret = GlobalObjects::danmuPool->getPool()->addPoolCode(
                    code.mid(code.startsWith("kikoplay:pool=")?14:15),
                    code.startsWith("kikoplay:anime="));
        if (ret) refreshItems();
        this->showMessage(ret?tr("Code Added"):tr("Code Error"),ret? NM_HIDE:NM_ERROR | NM_HIDE);
    });

    QGridLayout *blockGLayout=new QGridLayout(this);
    blockGLayout->setContentsMargins(0,0,0,0);
    blockGLayout->addWidget(shareCodeButton,0,0);
    blockGLayout->addWidget(addCodeButton,0,1);
    blockGLayout->addWidget(contentScrollArea,1,0,1,3);
    blockGLayout->setRowStretch(1,1);
    blockGLayout->setColumnStretch(2,1);
    blockGLayout->setContentsMargins(0, 0, 0, 0);

    setMinimumSize(300*logicalDpiX()/96,120*logicalDpiY()/96);
    setSizeSettingKey("DialogSize/PoolEdit",QSize(600*logicalDpiX()/96,320*logicalDpiY()/96));
}

void PoolEditor::refreshItems()
{
    for (auto item : poolItems)
    {
        item->deleteLater();
    }
    poolItems.clear();
    if (curPool)
    {
        curPool->disconnect(this);
        curPool = nullptr;
    }
    curPool = GlobalObjects::danmuPool->getPool();
    const auto &sources = curPool->sources();
    for (auto iter = sources.begin(); iter != sources.end(); ++iter)
    {
        PoolItem *poolItem = new PoolItem(&iter.value(),this);
        poolItems << poolItem;
        poolItemVLayout->insertWidget(0, poolItem);
    }
    QObject::connect(curPool, &Pool::poolChanged, this, &PoolEditor::refreshItems);
}


PoolItem::PoolItem(const DanmuSource *sourceInfo, QWidget *parent):QFrame(parent)
{
    QAction *viewDanmu=new QAction(tr("View Danmu"),this);
    QObject::connect(viewDanmu,&QAction::triggered,this,[sourceInfo](){
        DanmuView view(&GlobalObjects::danmuPool->getPool()->comments(),editor, sourceInfo->id);
        view.exec();
    });
    QAction *copyTimeline=new QAction(tr("Copy TimeLine Info"), this);
    QObject::connect(copyTimeline,&QAction::triggered,this,[sourceInfo](){
        if(sourceInfo->timelineInfo.empty()) return;
        timelineClipBoard=sourceInfo->timelineInfo;
        editor->showMessage(tr("The Timeline Info has been copied"));
    });
    QAction *pasteTimeline=new QAction(tr("Paste TimeLine Info"), this);
    QObject::connect(pasteTimeline,&QAction::triggered,this, [=](){
        if(timelineClipBoard.isEmpty()) return;
        auto timeline=sourceInfo->timelineInfo;
        for(auto &pair:timelineClipBoard)
        {
            int i=0;
            int c=timeline.count();
            while(i<c && timeline.at(i).first<pair.first) i++;
            if(i<c && timeline.at(i).first==pair.first) continue;
            timeline.insert(i,pair);
        }
        PoolSignalBlock block;
        GlobalObjects::danmuPool->getPool()->setTimeline(sourceInfo->id,timeline);
        editor->showMessage(tr("Pasted Timeline Info"));
    });
    setContextMenuPolicy(Qt::ActionsContextMenu);
    addAction(copyTimeline);
    addAction(pasteTimeline);
    addAction(viewDanmu);

    QFont normalFont(GlobalObjects::normalFont,16);

    QLabel *name=new QLabel(this);
    name->setFont(normalFont);
    QString sourceName = QString("%1(%2)").arg(sourceInfo->title).arg(sourceInfo->count);
    QString elidedName = name->fontMetrics().elidedText(sourceName, Qt::ElideMiddle, 600*logicalDpiX()/96);
    name->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    name->setText(elidedName);
    name->setToolTip(sourceName);

    QCheckBox *itemSwitch=new QCheckBox(tr("Show"),this);
    itemSwitch->setChecked(sourceInfo->show);
    QObject::connect(itemSwitch,&QCheckBox::stateChanged,[sourceInfo](int state){
        GlobalObjects::danmuPool->getPool()->setSourceVisibility(sourceInfo->id, state!=Qt::Unchecked);
    });
    QHBoxLayout *itemControlHLayout1=new QHBoxLayout();
    itemControlHLayout1->addWidget(name);
    itemControlHLayout1->addStretch(1);
    itemControlHLayout1->addWidget(itemSwitch);

    QLabel *url=new QLabel(tr("Source: %1%2").arg(sourceInfo->scriptId, sourceInfo->duration>0?QString(" (%1)").arg(sourceInfo->durationStr()):""),this);
    url->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Minimum);
    url->setToolTip(sourceInfo->scriptData);

    QLabel *delayLabel=new QLabel(tr("Delay(s): "),this);
    QSpinBox *delaySpinBox=new QSpinBox(this);
	delaySpinBox->setRange(INT_MIN,INT_MAX);
    delaySpinBox->setValue(sourceInfo->delay/1000);
    delaySpinBox->setObjectName(QStringLiteral("Delay"));
    delaySpinBox->setAlignment(Qt::AlignCenter);
    delaySpinBox->setFixedWidth(80*logicalDpiX()/96);
    QObject::connect(delaySpinBox,&QSpinBox::editingFinished,[delaySpinBox,sourceInfo](){
        PoolSignalBlock block;
        GlobalObjects::danmuPool->getPool()->setDelay(sourceInfo->id,delaySpinBox->value()*1000);
    });
    QPushButton *editTimeline=new QPushButton(tr("Edit Timeline"),this);
    //editTimeline->setFixedWidth(80*logicalDpiX()/96);
    QObject::connect(editTimeline,&QPushButton::clicked,[sourceInfo,this](){
        int curTime=GlobalObjects::mpvplayer->getTime();
        QVector<SimpleDanmuInfo> list;
        GlobalObjects::danmuPool->getPool()->exportSimpleInfo(sourceInfo->id,list);
        TimelineEdit timelineEdit(sourceInfo,list,this,curTime);
        if(QDialog::Accepted==timelineEdit.exec())
        {
            PoolSignalBlock block;
            GlobalObjects::danmuPool->getPool()->setTimeline(sourceInfo->id, timelineEdit.timelineInfo);
        }
    });

    QPushButton *deleteButton=new QPushButton(tr("Delete"),this);
    QObject::connect(deleteButton,&QPushButton::clicked,[this,sourceInfo](){
        PoolSignalBlock block;
        GlobalObjects::danmuPool->getPool()->deleteSource(sourceInfo->id);
        items->removeOne(this);
        this->deleteLater();
    });
    QPushButton *updateButton=new QPushButton(tr("Update"),this);
    QObject::connect(updateButton,&QPushButton::clicked,[this,sourceInfo,updateButton,deleteButton,
                     editTimeline,delaySpinBox,name](){
        QList<DanmuComment *> tmpList;
        updateButton->setEnabled(false);
        deleteButton->setEnabled(false);
        editTimeline->setEnabled(false);
        delaySpinBox->setEnabled(false);
        this->parentWidget()->setEnabled(false);
        editor->showBusyState(true);
        PoolSignalBlock block;
        int addCount = GlobalObjects::danmuPool->getPool()->update(sourceInfo->id);
        if(addCount>0)
        {
            editor->showMessage(tr("Add %1 New Danmu").arg(addCount));
            QString sourceName = QString("%1(%2)").arg(sourceInfo->title).arg(sourceInfo->count);
            QString elidedName = name->fontMetrics().elidedText(sourceName, Qt::ElideMiddle, 600*logicalDpiX()/96);
            name->setText(sourceName);
            name->setToolTip(elidedName);
        }
        editor->showBusyState(false);
        this->parentWidget()->setEnabled(true);
        updateButton->setEnabled(true);
        deleteButton->setEnabled(true);
        editTimeline->setEnabled(true);
        delaySpinBox->setEnabled(true);
    });

    QPushButton *exportButton=new QPushButton(tr("Export"),this);
    QObject::connect(exportButton,&QPushButton::clicked,[this,sourceInfo](){
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save Danmu"),sourceInfo->title,tr("Xml File (*.xml)"));
        if(!fileName.isEmpty())
        {
            GlobalObjects::danmuPool->getPool()->exportPool(fileName,true,true,QList<int>()<<sourceInfo->id);
        }
    });

    QFontMetrics fm(deleteButton->fontMetrics());
    int btnW = qMax(fm.horizontalAdvance(deleteButton->text()), fm.horizontalAdvance(updateButton->text()));
    btnW = qMax(btnW, fm.horizontalAdvance(exportButton->text())) * 1.5;
    deleteButton->setFixedWidth(btnW);
    updateButton->setFixedWidth(btnW);
    exportButton->setFixedWidth(btnW);

    QHBoxLayout *itemControlHLayout2=new QHBoxLayout();
    itemControlHLayout2->addWidget(delayLabel);
    itemControlHLayout2->addSpacing(8*logicalDpiX()/96);
    itemControlHLayout2->addWidget(delaySpinBox);
    itemControlHLayout2->addSpacing(4*logicalDpiX()/96);
    itemControlHLayout2->addWidget(editTimeline);
    itemControlHLayout2->addStretch(1);
    itemControlHLayout2->addWidget(updateButton);
    itemControlHLayout2->addWidget(exportButton);
    itemControlHLayout2->addWidget(deleteButton);

    QVBoxLayout *itemVLayout=new QVBoxLayout(this);
    itemVLayout->setSpacing(2*logicalDpiY()/96);
    itemVLayout->addLayout(itemControlHLayout1);
    itemVLayout->addWidget(url);
    itemVLayout->addLayout(itemControlHLayout2);
    //setFixedHeight(140);
    //setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Minimum);
    setObjectName(QStringLiteral("PoolItem"));
}
