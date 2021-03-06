
#include "dtagedit.h"
#include "../app/define.h"
#include "../tag/tagmanager.h"
#include "../utils/singleton.h"
#include "../app/filesignalmanager.h"
#include "../controllers/appcontroller.h"
#include "../interfaces/dfmeventdispatcher.h"

#include "dfileservices.h"

#include <QObject>
#include <QKeyEvent>
#include <QApplication>

DTagEdit::DTagEdit(QWidget *const parent)
    : DArrowRectangle{ DArrowRectangle::ArrowTop,  parent }
{
    this->initializeWidgets();
    this->initializeParameters();
    this->initializeLayout();
    this->initializeConnect();

    this->installEventFilter(this);

    setWindowFlags(Qt::Tool);
}

void DTagEdit::setFocusOutSelfClosing(bool value)noexcept
{
    ///###: CAS!
    bool excepted{ !value };
    m_flagForShown.compare_exchange_strong(excepted, value, std::memory_order_release);
}

void DTagEdit::setFilesForTagging(const QList<DUrl> &files)
{
    m_files = files;
}

void DTagEdit::setDefaultCrumbs(const QStringList &list)
{
    //由于crumbEdit变更响应被优化合并这里的改动不再被需要，先注释掉
    //TODO 等过几个版本这块功能没有问题了再删除该处冗余注释
    //设置初始值会按顺序添加，多标签情况下会多次触发processTag，这样可能会导致文件被移除再添加等不可控的问题出现
    //所以这里先断开信号槽链接 等list被append完毕之后主动调用processTag，再连接信号槽
//    QObject::disconnect(m_crumbEdit, &DCrumbEdit::crumbListChanged, this, &DTagEdit::processTags);
    for (const QString &crumb : list)
        m_crumbEdit->appendCrumb(crumb);
//    if (list.length() > 0)
//        processTags();
//    QObject::connect(m_crumbEdit, &DCrumbEdit::crumbListChanged, this, &DTagEdit::processTags);
}

void DTagEdit::onFocusOut()
{
    if (m_flagForShown.load(std::memory_order_acquire)) {
        this->processTags();
        this->close();
    }
}

void DTagEdit::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape: {
        this->processTags();
        event->accept();
        this->close();
        break;
    }
    case Qt::Key_Enter:
    case Qt::Key_Return: {
        QObject::disconnect(this, &DTagEdit::windowDeactivate, this, &DTagEdit::onFocusOut);
        this->processTags();
        event->accept();
        this->close();
        break;
    }
    default:
        break;
    }

    DArrowRectangle::keyPressEvent(event);
}

void DTagEdit::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void DTagEdit::initializeWidgets()
{
    m_crumbEdit = new DCrumbEdit;
    m_promptLabel = new QLabel{
        QObject::tr("Input tag info, such as work, family. A comma is used between two tags.")
    };
    m_totalLayout = new QVBoxLayout;
    m_BGFrame = new QFrame;
}

void DTagEdit::initializeParameters()
{
    m_crumbEdit->setFixedSize(140, 40);
    m_crumbEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_crumbEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_crumbEdit->setCrumbReadOnly(true);
    m_crumbEdit->setCrumbRadius(2);

    m_promptLabel->setFixedWidth(140);
    m_promptLabel->setWordWrap(true);
    m_BGFrame->setContentsMargins(QMargins{0, 0, 0, 0});

    this->setMargin(0);
    this->setFixedWidth(140);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setBorderColor(QColor{"#ffffff"});
    this->setBackgroundColor(QColor{"#ffffff"});
    this->setWindowFlags(Qt::FramelessWindowHint);
}

void DTagEdit::initializeLayout()
{
    m_totalLayout->addStretch(1);
    m_totalLayout->addWidget(m_crumbEdit);
    m_totalLayout->addSpacing(8);
    m_totalLayout->addWidget(m_promptLabel);
    m_totalLayout->addStretch(1);

    m_BGFrame->setLayout(m_totalLayout);
    this->setContent(m_BGFrame);
}

void DTagEdit::initializeConnect()
{
    QObject::connect(this, &DTagEdit::windowDeactivate, this, &DTagEdit::onFocusOut);

    //为避免crumbEdit频繁变更 导致processTags被高频大量调用 这里做一个变更合并机制
    m_waitForMoreCrumbChanged.setSingleShot(true);
    QObject::connect(m_crumbEdit, &DCrumbEdit::crumbListChanged, this, [=]{
        m_waitForMoreCrumbChanged.start(500);   //500毫秒内的crumb连续变更会合并为一次crumb变更操作
    });
    QObject::connect(&m_waitForMoreCrumbChanged, &QTimer::timeout, this, &DTagEdit::processTags);
}

void DTagEdit::processTags()
{
    QList<QString> tagList{ m_crumbEdit->crumbList() };
    //防止DTagEdit对象被析构后m_files无法访问
    QList<DUrl> files = m_files;

    DFileService::instance()->makeTagsOfFiles(this, files, tagList);
}
