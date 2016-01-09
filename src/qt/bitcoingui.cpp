/*
 * Qt5 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */
#include "init.h"
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "sendbitcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "accessnxtinsidedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "postdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "transactionview.h"
#include "transactionspage.h"
#include "overviewpage.h"
#include "bitcoinunits.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"
#include "askpassphrasepage.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "getlecoinpage.h"
#include "forumspage.h"
#include "chatpage.h"
#include "blockchainpage.h"
#include "supernetpage.h"
#include "ui_getlecoinpage.h"
#include "ui_forumspage.h"
#include "ui_chatpage.h"
#include "ui_blockchainpage.h"
#include "ui_supernetpage.h"
#include "downloader.h"
#include "updatedialog.h"
#include "whatsnewdialog.h"
#include "rescandialog.h"
#include "cookiejar.h"
#include "webview.h"

#include "JlCompress.h"
#include "walletdb.h"
#include "wallet.h"
#include "txdb.h"
#include <boost/version.hpp>
#include <boost/filesystem.hpp>

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QDesktopWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QFileDialog>
#include <QTimer>
#include <QDragEnterEvent>
#include <QUrl>
#include <QStyle>
#include <QFontDatabase>
#include <QInputDialog>
#include <QGraphicsView>

#include <iostream>

using namespace GUIUtil;

extern CWallet* pwalletMain;
extern int64_t nLastCoinStakeSearchInterval;
extern unsigned int nTargetSpacing;
double GetPoSKernelPS();
bool blocksIcon = true;
bool resizeGUICalled = false;


BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    currentTotal(-1),
    changePassphraseAction(0),
    lockWalletAction(0),
    unlockWalletAction(0),
    encryptWalletAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0)
{
    QDesktopWidget desktop;
    QRect screenSize = desktop.availableGeometry(desktop.primaryScreen());
    //QRect screenSize = QRect(0, 0, 1024, 728); // SDW DEBUG
    if (screenSize.height() <= WINDOW_MIN_HEIGHT)
    {
        GUIUtil::refactorGUI(screenSize);
    }
    setMinimumSize(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);
    resizeGUI();
    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), screenSize));

    QFontDatabase::addApplicationFont(":fonts/Lato-Bold");
    QFontDatabase::addApplicationFont(":fonts/Lato-Regular");
    GUIUtil::setFontPixelSizes();
    qApp->setFont(qFont);

    setWindowTitle(tr("LeCoin Wallet"));
    setWindowIcon(QIcon(":icons/bitcoin"));
    qApp->setWindowIcon(QIcon(":icons/bitcoin"));

    qApp->setStyleSheet(veriStyleSheet);

/* (Seems to be working in Qt5)
#ifdef Q_OS_MAC
    setUnifiedTitleAndToolBarOnMac(false);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
*/
    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create the tray icon (or setup the dock icon)
    createTrayIcon();

    // Create AskPassphrase Page
    askPassphrasePage = new AskPassphrasePage(AskPassphrasePage::Unlock, this);
    encryptWalletPage = new AskPassphrasePage(AskPassphrasePage::Encrypt, this);

    // Create Overview Page
    overviewPage = new OverviewPage();

    // Create Send Page
    sendCoinsPage = new SendCoinsDialog(this);

    // Create Receive Page
    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);
    // Re-set Header and styles for Receive (Default is headerAddress)
    if (fNoHeaders)
        receiveCoinsPage->findChild<QGraphicsView *>("header")->setStyleSheet("QGraphicsView { background-color: " + STR_COLOR + "; }");
    else if (fSmallHeaders)
        receiveCoinsPage->findChild<QGraphicsView *>("header")->setStyleSheet("QGraphicsView { background: url(:images/headerReceiveSmall) no-repeat 0px 0px; border: none; background-color: " + STR_COLOR + "; }");
    else
        receiveCoinsPage->findChild<QGraphicsView *>("header")->setStyleSheet("QGraphicsView { background: url(:images/headerReceive) no-repeat 0px 0px; border: none; background-color: " + STR_COLOR + "; }");

    // Create History Page
    transactionsPage = new TransactionsPage();

    /* Build the transaction view then pass it to the transaction page to share */
    QVBoxLayout *vbox = new QVBoxLayout();
    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    vbox->setContentsMargins(10, 10 + HEADER_HEIGHT, 10, 10);
    transactionsPage->setLayout(vbox);

    // Create Address Page
    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::AddressBookTab);

    // Create VeriBit Page
    sendBitCoinsPage = new SendBitCoinsDialog(this);

    // Create GetLeCoin Page
    getlecoinPage = new GetlecoinPage();

    // Create Forums Page
    forumsPage = new ForumsPage();

    // Create Chat Page
    chatPage = new ChatPage();

    // Create Blockchain Page
    blockchainPage = new BlockchainPage();

    // Create SuperNET Page
    if (!fSuperNETInstalled)
    {
        superNETPage = new SuperNETPage();
    }
    else
    {
        // Place holder for SuperNET gateway
        superNETPage = new SuperNETPage();
    }

    // Create Sign Message Dialog
    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    // Create SuperNET Dialog
    accessNxtInsideDialog = new AccessNxtInsideDialog(this);

    centralWidget = new QStackedWidget(this);
    centralWidget->setFrameShape(QFrame::NoFrame);
    centralWidget->addWidget(askPassphrasePage);
    centralWidget->addWidget(encryptWalletPage);
    centralWidget->addWidget(overviewPage);
    centralWidget->addWidget(transactionsPage);
    //centralWidget->addWidget(addressBookPage);
    centralWidget->addWidget(receiveCoinsPage);
    centralWidget->addWidget(sendCoinsPage);
    centralWidget->addWidget(sendBitCoinsPage);
    centralWidget->addWidget(getlecoinPage);
    centralWidget->addWidget(forumsPage);
    centralWidget->addWidget(chatPage);
    centralWidget->addWidget(blockchainPage);
    centralWidget->addWidget(superNETPage);
    setCentralWidget(centralWidget);

    // Create status bar
    statusBar();
    statusBar()->setContentsMargins(STATUSBAR_MARGIN,0,0,0);
    statusBar()->setFont(qFontSmall);
    statusBar()->setFixedHeight(STATUSBAR_HEIGHT);

    QFrame *versionBlocks = new QFrame();
    versionBlocks->setContentsMargins(0,0,0,0);
    versionBlocks->setFont(qFontSmall);

    versionBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *versionBlocksLayout = new QHBoxLayout(versionBlocks);
    versionBlocksLayout->setContentsMargins(0,0,0,0);
    versionBlocksLayout->setSpacing(6);

    labelVersionIcon = new QLabel();
    labelVersionIcon->setContentsMargins(0,0,0,0);
    labelVersionIcon->setPixmap(QIcon(":/icons/statusGood").pixmap(4, STATUSBAR_ICONSIZE));
    versionLabel = new QLabel();
    versionLabel->setContentsMargins(0,0,0,0);
    if (!STATUSBAR_MARGIN)
        versionLabel->setFont(qFontSmallest);
    else
        versionLabel->setFont(qFontSmaller);
    versionLabel->setFixedWidth(TOOLBAR_WIDTH - STATUSBAR_MARGIN - (versionBlocksLayout->spacing() * 3) - labelVersionIcon->pixmap()->width());
    versionLabel->setText(tr("版本 %1").arg(FormatVersion(CLIENT_VERSION).c_str()));
    versionLabel->setStyleSheet("QLabel { color: white; }");

    versionBlocksLayout->addWidget(labelVersionIcon);
    versionBlocksLayout->addWidget(versionLabel);

    balanceLabel = new QLabel();
    balanceLabel->setFont(qFontSmall);
    balanceLabel->setText(QString(""));
    balanceLabel->setFixedWidth(FRAMEBLOCKS_LABEL_WIDTH);

    stakingLabel = new QLabel();
    stakingLabel->setFont(qFontSmall);
    stakingLabel->setText(QString("自动同步..."));
    QFontMetrics fm(stakingLabel->font());
    int labelWidth = fm.width(stakingLabel->text());
    stakingLabel->setFixedWidth(labelWidth + 10);

    connectionsLabel= new QLabel();
    connectionsLabel->setFont(qFontSmall);
    connectionsLabel->setText(QString("连接节点..."));
    connectionsLabel->setFixedWidth(FRAMEBLOCKS_LABEL_WIDTH);

    labelBalanceIcon = new QLabel();
    labelBalanceIcon->setPixmap(QIcon(":/icons/balance").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
    labelStakingIcon = new QLabel();
    labelStakingIcon->setVisible(false);
    labelConnectionsIcon = new QLabel();
    labelConnectionsIcon->setFont(qFontSmall);
    labelBlocksIcon = new QLabel();
    labelBlocksIcon->setVisible(true);
    labelBlocksIcon->setPixmap(QIcon(":/icons/notsynced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setFont(qFontSmall);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    frameBlocks->setStyleSheet("QFrame { color: white; }");
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,3,3,3);
    frameBlocksLayout->setSpacing(10);

    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBalanceIcon);
    frameBlocksLayout->addWidget(balanceLabel);
    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addWidget(stakingLabel);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addWidget(connectionsLabel);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBar = new QProgressBar();
    progressBar->setContentsMargins(0,0,0,0);
    progressBar->setFont(qFontSmall);
    progressBar->setMinimumWidth(550);
    progressBar->setStyleSheet("QProgressBar::chunk { background: " + STR_COLOR_LT + "; } QProgressBar { color: black; border-color: " + STR_COLOR_LT + "; margin: 3px; margin-right: 13px; border-width: 1px; border-style: solid; }");
    progressBar->setAlignment(Qt::AlignCenter);
    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = qApp->style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background: white; color: black; border: 0px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 " + STR_COLOR_LT + "); border-radius: 7px; margin: 0px; }");
    }
    progressBar->setVisible(true);

    statusBar()->addWidget(versionBlocks);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    syncIconMovie = new QMovie(":/movies/update_spinner", "mng", this);

    if (GetBoolArg("权益中", true))
    {
        QTimer *timerStakingIcon = new QTimer(labelStakingIcon);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingIcon()));
        timerStakingIcon->start(30 * 1000);
        updateStakingIcon();
    }

    // Set a timer to check for updates daily
    QTimer *tCheckForUpdate = new QTimer(this);
    connect(tCheckForUpdate, SIGNAL(timeout()), this, SLOT(timerCheckForUpdate()));
    tCheckForUpdate->start(24 * 60 * 60 * 1000); // every 24 hours

    connect(askPassphrasePage, SIGNAL(lockWalletFeatures(bool)), this, SLOT(lockWalletFeatures(bool)));
    connect(encryptWalletPage, SIGNAL(lockWalletFeatures(bool)), this, SLOT(lockWalletFeatures(bool)));
    connect(sendCoinsPage, SIGNAL(gotoSendBitCoins()), this, SLOT(gotoSendBitCoinsPage()));
    connect(sendBitCoinsPage, SIGNAL(gotoSendCoins()), this, SLOT(gotoSendCoinsPage()));

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    // Clicking on "Verify Message" in the address book sends you to the verify message tab
    connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
    // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
    connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));

    // Clicking on "Access Nxt Inside" in the receive coins page sends you to access Nxt inside tab
	connect(receiveCoinsPage, SIGNAL(accessNxt(QString)), this, SLOT(gotoAccessNxtInsideTab(QString)));
}

BitcoinGUI::~BitcoinGUI()
{
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
#endif
}

void BitcoinGUI::logout()
{
    lockWallet();
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        lockWalletFeatures(true);
    }
}

void BitcoinGUI::lockWalletFeatures(bool lock)
{
    if (lock && !fTestNet)
    {
        appMenuBar->setVisible(false);
        toolbar->setVisible(false);
        statusBar()->setVisible(false);

        this->setWindowState(Qt::WindowNoState); // Fix for window maximized state
        resizeGUI();

        if (walletModel && walletModel->getEncryptionStatus() == WalletModel::Unencrypted)
            gotoEncryptWalletPage();
        else
            gotoAskPassphrasePage();
    }
    else
    {
        gotoOverviewPage();

        QSettings settings("LeCoin", "LeCoin-Qt");
        restoreGeometry(settings.value("geometry").toByteArray());
        restoreState(settings.value("windowState").toByteArray());

        appMenuBar->setVisible(true);
        toolbar->setVisible(true);
        statusBar()->setVisible(true);
    }

    // Hide/Show every action in tray but Exit
    QList<QAction *> trayActionItems = trayIconMenu->actions();
    foreach (QAction* ai, trayActionItems) {
        ai->setVisible(lock == false);
    }
    toggleHideAction->setVisible(true);
    quitAction->setVisible(true);
}

void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("硬币概括"), this);
    overviewAction->setToolTip(tr("钱包主页"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("发送硬币"), this);
    sendCoinsAction->setToolTip(tr("发送乐币"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    /* Removed tab to simplify wallet
    sendBitCoinsAction = new QAction(QIcon(":/icons/veriBit"), tr("VeriBit"), this);
    sendBitCoinsAction->setToolTip(tr("Send Bitcoin"));
    sendBitCoinsAction->setCheckable(true);
    sendBitCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendBitCoinsAction);
    */

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("接收硬币"), this);
    receiveCoinsAction->setToolTip(tr("收款您的乐币地址"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("交易历史"), this);
    historyAction->setToolTip(tr("乐币交易历史"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    /* Removed tab to simplify wallet
    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("Address"), this);
    addressBookAction->setToolTip(tr("Saved Addresses"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(addressBookAction);
    */

    getlecoinAction = new QAction(QIcon(":/icons/getlecoin"), tr("免费获取"), this);
    getlecoinAction->setToolTip(tr("推广就能获得免费乐币"));
    getlecoinAction->setCheckable(true);
    getlecoinAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    tabGroup->addAction(getlecoinAction);

    forumsAction = new QAction(QIcon(":/icons/forums"), tr("乐币论坛"), this);
    forumsAction->setToolTip(tr("加入乐币论坛"));
    forumsAction->setCheckable(true);
    forumsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_7));
    tabGroup->addAction(forumsAction);

    chatAction = new QAction(QIcon(":/icons/chat"), tr("区块查询"), this);
    chatAction->setToolTip(tr("乐币区块连查询"));
    chatAction->setCheckable(true);
    chatAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_8));
    tabGroup->addAction(chatAction);

    blockchainAction = new QAction(QIcon(":/icons/blockchain"), tr("乐币扩展"), this);
    blockchainAction->setToolTip(tr("乐币扩展中心"));
    blockchainAction->setCheckable(true);
    blockchainAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_9));
    tabGroup->addAction(blockchainAction);

    superNETAction = new QAction(QIcon(":/icons/supernet_white"), tr("交易系统"), this);
    superNETAction->setToolTip(tr("大型交易所"));
    superNETAction->setCheckable(true);
    superNETAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_0));
    tabGroup->addAction(superNETAction);

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    //connect(sendBitCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    //connect(sendBitCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendBitCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    //connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    //connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));
    connect(getlecoinAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(getlecoinAction, SIGNAL(triggered()), this, SLOT(gotoGetlecoinPage()));
    connect(forumsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(forumsAction, SIGNAL(triggered()), this, SLOT(gotoForumsPage()));
    connect(chatAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(chatAction, SIGNAL(triggered()), this, SLOT(gotoChatPage()));
    connect(blockchainAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(blockchainAction, SIGNAL(triggered()), this, SLOT(gotoBlockchainPage()));
    connect(superNETAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(superNETAction, SIGNAL(triggered()), this, SLOT(gotoSuperNETPage()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("退出"), this);
    quitAction->setToolTip(tr("退出应用程序"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    logoutAction = new QAction(QIcon(":/icons/logout"), tr("&登出"), this);
    logoutAction->setToolTip(tr("登出和停止POS"));
    logoutAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_L));
    aboutAction = new QAction(QIcon(":/icons/about"), tr("&关于乐币"), this);
    aboutAction->setToolTip(tr("显示有关乐币信息"));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutPostAction = new QAction(QIcon(":/icons/PoSTicon"), tr("&关于POST"), this);
    aboutPostAction->setToolTip(tr("显示有关POST协议信息"));
    aboutPostAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(QIcon(":icons/about-qt"), tr("关于 &Qt"), this);
    aboutQtAction->setToolTip(tr("显示关于Qt信息"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&选项"), this);
    optionsAction->setToolTip(tr("修改乐币配置选项"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/bitcoin"), tr("&显示/隐藏"), this);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&备份钱包"), this);
    backupWalletAction->setToolTip(tr("钱包备份到另一个位置"));
    rescanWalletAction = new QAction(QIcon(":/icons/rescan"), tr("扫描钱包"), this);
    rescanWalletAction->setToolTip(tr("重新扫描区块链你的钱包交易."));
    reloadBlockchainAction = new QAction(QIcon(":/icons/blockchain-dark"), tr("&区块下载"), this);
    reloadBlockchainAction->setToolTip(tr("重新装载引导的区块."));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&更改密码"), this);
    changePassphraseAction->setToolTip(tr("更改用于钱包加密的密码"));
    lockWalletAction = new QAction(QIcon(":/icons/staking_off"), tr("&停止POS"), this);
    lockWalletAction->setToolTip(tr("关闭POS锁定钱包"));
    unlockWalletAction = new QAction(QIcon(":/icons/staking_on"), tr("&开始POS"), this);
    unlockWalletAction->setToolTip(tr("开始POS"));
    encryptWalletAction = new QAction(QIcon(":/icons/lock_open"), tr("加密钱包"), this);
    encryptWalletAction->setToolTip(tr("加密钱包POS"));
    addressBookAction = new QAction(QIcon(":/icons/address-book-menu"), tr("&地址簿"), this);
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("签名地址"), this);
    verifyMessageAction = new QAction(QIcon(":/icons/verify"), tr("&验证信息"), this);
    //accessNxtInsideAction = new QAction(QIcon(":/icons/supernet"), tr("查看计划"), this);
    checkForUpdateAction = new QAction(QIcon(":/icons/update"), tr("升级钱包"), this);
    checkForUpdateAction->setToolTip(tr("点击新版本升级."));
    forumAction = new QAction(QIcon(":/icons/bitcoin"), tr("乐币"), this);
    forumAction->setToolTip(tr("转至乐币论坛."));
    webAction = new QAction(QIcon(":/icons/site"), tr("官方网站"), this);
    webAction->setToolTip(tr("打开乐币官网."));

    exportAction = new QAction(QIcon(":/icons/export"), tr("&导出数据"), this);
    exportAction->setToolTip(tr("导出当前选项卡中的数据文件"));
    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&控制台"), this);
    openRPCConsoleAction->setToolTip(tr("打开调试和诊断控制台"));

    connect(quitAction, SIGNAL(triggered()), this, SLOT(exitApp()));
    connect(logoutAction, SIGNAL(triggered()), this, SLOT(logout()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutPostAction, SIGNAL(triggered()), this, SLOT(aboutPostClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(rescanWalletAction, SIGNAL(triggered()), this, SLOT(rescanWallet()));
    connect(reloadBlockchainAction, SIGNAL(triggered()), this, SLOT(reloadBlockchain()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(lockWalletAction, SIGNAL(triggered()), this, SLOT(lockWallet()));
    connect(unlockWalletAction, SIGNAL(triggered()), this, SLOT(unlockWallet()));
    connect(encryptWalletAction, SIGNAL(triggered()), this, SLOT(encryptWallet()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
    //connect(accessNxtInsideAction, SIGNAL(triggered()), this, SLOT(gotoAccessNxtInsideTab()));
    connect(checkForUpdateAction, SIGNAL(triggered()), this, SLOT(menuCheckForUpdate()));
    connect(forumAction, SIGNAL(triggered()), this, SLOT(forumClicked()));
    connect(webAction, SIGNAL(triggered()), this, SLOT(webClicked()));

    // Disable on testnet
    if (fTestNet)
        reloadBlockchainActionEnabled(false);
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif
    appMenuBar->setFont(qFont);

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->setFont(qFont);
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
    file->addAction(rescanWalletAction);
    file->addAction(reloadBlockchainAction);
    file->addSeparator();
    file->addAction(addressBookAction);
    file->addAction(signMessageAction);
    //file->addAction(verifyMessageAction);
    //file->addSeparator();
    //file->addAction(accessNxtInsideAction);
    file->addSeparator();
    file->addAction(logoutAction);
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->setFont(qFont);
    settings->addAction(lockWalletAction);
    settings->addAction(unlockWalletAction);
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->setFont(qFont);
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(webAction);
    help->addAction(checkForUpdateAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutPostAction);
    help->addAction(aboutQtAction);
}

void BitcoinGUI::createToolBars()
{
    toolbar = addToolBar(tr("标签工具栏"));
    toolbar->setObjectName(QStringLiteral("工具栏"));
    addToolBar(Qt::LeftToolBarArea, toolbar);
    toolbar->setMovable(false);
    toolbar->setAutoFillBackground(true);
    toolbar->setContentsMargins(0,0,0,0);
    toolbar->layout()->setSpacing(0);
    toolbar->setOrientation(Qt::Vertical);
    toolbar->setIconSize(QSize(TOOLBAR_ICON_WIDTH,TOOLBAR_ICON_HEIGHT));
    toolbar->setFixedWidth(TOOLBAR_WIDTH);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    //toolbar->addAction(addressBookAction);
    //toolbar->addAction(sendBitCoinsAction);
    toolbar->addAction(getlecoinAction);
    toolbar->addAction(forumsAction);
    toolbar->addAction(chatAction);
    toolbar->addAction(blockchainAction);
    toolbar->addAction(superNETAction);
}

void BitcoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        // Replace some strings and icons, when using the testnet
        if(clientModel->isTestNet())
        {
            setWindowTitle(windowTitle() + QString(" ") + tr("[PoST testnet]"));
#ifndef Q_OS_MAC
            qApp->setWindowIcon(QIcon(":icons/bitcoin_testnet"));
            setWindowIcon(QIcon(":icons/bitcoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin_testnet"));
#endif
            if(trayIcon)
            {
                trayIcon->setToolTip(tr("LeCoin Wallet") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
                toggleHideAction->setIcon(QIcon(":/icons/toolbar_testnet"));
            }
        }

        // Set version icon good/bad
        setVersionIcon(fNewVersion);
        connect(clientModel, SIGNAL(versionChanged(bool)), this, SLOT(setVersionIcon(bool)));

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int)));

        // Report errors from network/worker thread
        connect(clientModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        rpcConsole->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
    }
}

void BitcoinGUI::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    if(walletModel)
    {
        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        // Put transaction list in tabs
        askPassphrasePage->setModel(walletModel);
        encryptWalletPage->setModel(walletModel);
        overviewPage->setModel(walletModel);
        sendCoinsPage->setModel(walletModel);
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        transactionView->setModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        sendBitCoinsPage->setModel(walletModel);
        getlecoinPage->setModel(walletModel);
        forumsPage->setModel(walletModel);
        chatPage->setModel(walletModel);
        blockchainPage->setModel(walletModel);
        superNETPage->setModel(walletModel);

        signVerifyMessageDialog->setModel(walletModel);
        //accessNxtInsideDialog->setModel(walletModel);

        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

        // Set balance in status bar
        connect(walletModel, SIGNAL(balanceChanged(qint64,qint64,qint64,qint64)), this, SLOT(setBalanceLabel(qint64,qint64,qint64,qint64)));
        setBalanceLabel(walletModel->getBalance(), walletModel->getStake(), walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance());

        // Passphrase required.
        lockWalletFeatures(true); // Lock features
    }
}

void BitcoinGUI::createTrayIcon()
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip(tr("乐币钱包"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(logoutAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addAction(quitAction);
#endif

    notificator = new Notificator(qApp->applicationName(), trayIcon);
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void BitcoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();

    // force a balance update instead of waiting on timer
    setBalanceLabel(walletModel->getBalance(), walletModel->getStake(), walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance());
}

void BitcoinGUI::forumClicked()
{
    QDesktopServices::openUrl(QUrl(forumsUrl));
}

void BitcoinGUI::webClicked()
{
    QDesktopServices::openUrl(QUrl(walletUrl));
}

void BitcoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void BitcoinGUI::aboutPostClicked()
{
    PostDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void BitcoinGUI::setBalanceLabel(qint64 balance, qint64 stake, qint64 unconfirmed, qint64 immature)
{
    if (clientModel && walletModel)
    {
        qint64 total = balance + stake + unconfirmed + immature;
        QString balanceStr = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), balance, false, walletModel->getOptionsModel()->getHideAmounts());
        QString stakeStr = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), stake, false, walletModel->getOptionsModel()->getHideAmounts());
        QString unconfirmedStr = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), unconfirmed, false, walletModel->getOptionsModel()->getHideAmounts());
        //QString immatureStr = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), immature, false, walletModel->getOptionsModel()->getHideAmounts());
        balanceLabel->setText(BitcoinUnits::formatWithUnitWithMaxDecimals(walletModel->getOptionsModel()->getDisplayUnit(), total, walletModel->getOptionsModel()->getDecimalPoints(), false, walletModel->getOptionsModel()->getHideAmounts()));
        labelBalanceIcon->setToolTip(tr("可用乐币: %1\n权益累积: %2\n未确认: %3").arg(balanceStr).arg(stakeStr).arg(unconfirmedStr));
        QFontMetrics fm(balanceLabel->font());
        int labelWidth = fm.width(balanceLabel->text());
        balanceLabel->setFixedWidth(labelWidth + 20);
        if (total > currentTotal)
        {
            balanceLabel->setStyleSheet("QLabel { color: #009966; }");
        }
        else if (total < currentTotal)
        {
            balanceLabel->setStyleSheet("QLabel { color: orange; }");
        }
        else
        {
            balanceLabel->setStyleSheet("QLabel { color: white; }");
        }
        currentTotal = total;
    }
}

void BitcoinGUI::setVersionIcon(bool newVersion)
{
    QString icon;
    switch(newVersion)
    {
        case true: icon = ":/icons/statusBad"; versionLabel->setStyleSheet("QLabel {color: red;}"); break;
        case false: icon = ":/icons/statusGood"; versionLabel->setStyleSheet("QLabel {color: white;}"); break;
    }
    labelVersionIcon->setPixmap(QIcon(icon).pixmap(72,STATUSBAR_ICONSIZE));
    labelVersionIcon->setToolTip(newVersion ? tr("你的钱包太老了！\ n下载最新版本的帮助.") : tr("你是最新的钱包版本."));
}

void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
        case 0: icon = ":/icons/connect_0"; break;
        case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
        case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
        case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
        default: icon = ":/icons/connect_4"; break;
    }
    QString connections = QString::number(count);
    QString label = " 连接节点";
    QString connectionlabel = connections + label;
    connectionsLabel->setText(QString(connectionlabel));
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(72,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%1 活动连接%2 到乐币网络").arg(count).arg(count == 1 ? "" : "s"));
}

void BitcoinGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // don't show / hide progress bar if we have no connection to the network
    if (!clientModel || clientModel->getNumConnections() == 0)
    {
        progressBar->setVisible(true);
        progressBar->setFormat(tr("等待全网同步中..."));
        progressBar->setMaximum(nTotalBlocks);
        progressBar->setValue(0);
        progressBar->setVisible(true);
        progressBar->setToolTip(tr("全网同步中"));

        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip;

    if(count < nTotalBlocks)
    {
        int nRemainingBlocks = nTotalBlocks - count;
        float nPercentageDone = count / (nTotalBlocks * 0.01f);

        if (strStatusBarWarnings.isEmpty())
        {
            progressBar->setFormat(tr("网络同步: ~%1 块%2 剩余").arg(nRemainingBlocks).arg(nRemainingBlocks == 1 ? "" : "s"));
            progressBar->setMaximum(nTotalBlocks);
            progressBar->setValue(count);
            progressBar->setVisible(true);
        }

        tooltip = tr("下载 %1 of %2 交易历史 (%3% done).").arg(count).arg(nTotalBlocks).arg(nPercentageDone, 0, 'f', 2);
    }
    else
    {
        progressBar->setVisible(false);
        tooltip = tr("下载 %1 交易历史.").arg(count);
    }


    // Show a warning message if out of sync more than 500 blocks but not if more than 5000.
    int countDiff = nTotalBlocks - count;
    if ((countDiff > 500 && countDiff < 5000) && !fBootstrapTurbo && strStatusBarWarnings.isEmpty() && !clientModel->isTestNet())
    {
        strStatusBarWarnings = tr("不用担心>正常同步中,加快或解决同步问题请移步文件区>选择区块下载");
    }

    // Override progressBar text when we have warnings to display
    if (!strStatusBarWarnings.isEmpty())
    {
        progressBar->setFormat(strStatusBarWarnings);
        progressBar->setValue(0);
        progressBar->setVisible(true);

    }

    // Show Alert message always.
    if (GetBoolArg("-vAlert") && GetArg("-vAlertMsg","").c_str() != "")
    {
        // Add a delay in case there is another warning
        this->repaint();
        MilliSleep(2000);
        strStatusBarWarnings = tr(GetArg("-vAlertMsg","").c_str());
        progressBar->setFormat(strStatusBarWarnings);
        progressBar->setValue(0);
        progressBar->setVisible(true);
    }

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(QDateTime::currentDateTime());
    QString text;

    // Represent time from last generated block in human readable text
    if(secs <= 0)
    {
        // Fully up to date. Leave text empty.
    }
    else if(secs < 60)
    {
        text = tr("%n 秒前","",secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%n 分钟前","",secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%n 小时前","",secs/(60*60));
    }
    else
    {
        text = tr("%n 天前","",secs/(60*60*24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
        //tooltip = tr("Up to date") + QString(".\n") + tooltip;
        //labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        //labelBlocksIcon->hide();
        overviewPage->showOutOfSyncWarning(false);
    }
    else
    {
        stakingLabel->setText(QString("同步中..."));
        labelStakingIcon->hide();
        labelBlocksIcon->show();
        tooltip = tr("同步中") + QString(".\n") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/notsynced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        overviewPage->showOutOfSyncWarning(true);
    }

    if(!text.isEmpty())
    {
        tooltip += QString("\n");
        tooltip += tr("生成最后接收到的块 %1.").arg(text);
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("") + tooltip + QString("");

    labelBlocksIcon->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcoinGUI::error(const QString &title, const QString &message, bool modal)
{
    // Report errors from network/worker thread
    if(modal)
    {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}

void BitcoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
//#ifndef Q_OS_MAC // Ignored on Mac (Seems to be working in Qt5)
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
//#endif
}

void BitcoinGUI::exitApp()
{
    QSettings settings("LeCoin", "LeCoin-Qt");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    // Check if a pending bootstrap needs a restart (ie. user closed window before encrypting wallet)
    if (walletModel && fBootstrapTurbo)
    {
        if (!walletModel->reloadBlockchain())
        {
            fBootstrapTurbo = false;
            QMessageBox::warning(this, tr("刷新失败"), tr("有一个错误尝试重新加载区块."));
        }
    }
    else
    {
        qApp->quit();
        MilliSleep(500);
    }
}

void BitcoinGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
//#ifndef Q_OS_MAC // Ignored on Mac (Seems to be working in Qt5)
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
            !clientModel->getOptionsModel()->getMinimizeOnClose())
         {
            exitApp();
         }
//#endif
    }
    QMainWindow::closeEvent(event);
}

void BitcoinGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
          "which goes to the nodes that process your transaction and helps to support the network.  "
          "Do you want to pay the fee?").arg(
                BitcoinUnits::formatWithUnitFee(BitcoinUnits::LEC, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
          this, tr("确认交易费"), strMessage,
          QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    if(!walletModel || !clientModel)
        return;
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                    .data(Qt::EditRole).toULongLong();
    if(!clientModel->inInitialBlockDownload())
    {
        BitcoinUnits *bcu = new BitcoinUnits(this, walletModel);
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
                        .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
                        .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
                        .data().toString();
        QIcon icon = qvariant_cast<QIcon>(ttm->index(start,
                            TransactionTableModel::ToAddress, parent)
                        .data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
                            (amount)<0 ? tr("发送交易") :
                              tr("输入的交易"),
                              tr("时间: %1\n"
                              "金额: %2\n"
                              "类型: %3\n"
                              "地址: %4\n")
                              .arg(date)
                              .arg(bcu->formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
                              .arg(type)
                              .arg(address), icon);
        delete bcu;
    }
}

void BitcoinGUI::gotoAskPassphrasePage()
{
    overviewAction->setChecked(false);
    centralWidget->setCurrentWidget(askPassphrasePage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoEncryptWalletPage()
{
    fEncrypt = true;

    overviewAction->setChecked(false);
    centralWidget->setCurrentWidget(encryptWalletPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    centralWidget->setCurrentWidget(overviewPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoSendCoinsPage()
{
    sendCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(receiveCoinsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), receiveCoinsPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    centralWidget->setCurrentWidget(transactionsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), transactionView, SLOT(exportClicked()));
}

void BitcoinGUI::gotoAddressBookPage()
{
    if(!walletModel)
        return;

    AddressBookPage dlg(AddressBookPage::ForEditing, AddressBookPage::AddressBookTab, this);
    dlg.setModel(walletModel->getAddressTableModel());
    dlg.exec();

    /* Removed tab to simplify wallet
    addressBookAction->setChecked(true);
    centralWidget->setCurrentWidget(addressBookPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), addressBookPage, SLOT(exportClicked()));
    */
}

void BitcoinGUI::gotoSendBitCoinsPage()
{
    sendCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendBitCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);

    /* Combined tabs to simplify wallet
    sendBitCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendBitCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    */
}

void BitcoinGUI::gotoGetlecoinPage()
{
    getlecoinAction->setChecked(true);
    centralWidget->setCurrentWidget(getlecoinPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoForumsPage()
{
    forumsAction->setChecked(true);
    centralWidget->setCurrentWidget(forumsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoChatPage()
{
    chatAction->setChecked(true);
    centralWidget->setCurrentWidget(chatPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoBlockchainPage()
{
    blockchainAction->setChecked(true);
    centralWidget->setCurrentWidget(blockchainPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoSuperNETPage()
{
    superNETAction->setChecked(true);
    centralWidget->setCurrentWidget(superNETPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::resizeEvent(QResizeEvent *e)
{
    if (resizeGUICalled) return;  // Don't allow resizeEvent to be called twice

    if (e->size().height() < WINDOW_MIN_HEIGHT + 50 && e->size().width() < WINDOW_MIN_WIDTH + 50)
    {
        resizeGUI(); // snap to normal size wallet if within 50 pixels
    }
    else
    {
        resizeGUICalled = false;
    }
}

void BitcoinGUI::resizeGUI()
{
    resizeGUICalled = true;

    resize(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);

    resizeGUICalled = false;
}

void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);

}

void BitcoinGUI::gotoAccessNxtInsideTab(QString addr)
{
    // call show() in showTab_AN()
    accessNxtInsideDialog->showTab_AN(true);

    if(!addr.isEmpty())
        accessNxtInsideDialog->setAddress_AN(addr);
}

void BitcoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        int nValidUrisFound = 0;
        int nValidUrisFoundBit = 0;
        QList<QUrl> uris = event->mimeData()->urls();
        foreach(const QUrl &uri, uris)
        {
            if (sendCoinsPage->handleURI(uri.toString()))
                nValidUrisFound++;
            else if (sendBitCoinsPage->handleURI(uri.toString()))
                nValidUrisFoundBit++;
        }

        // if valid URIs were found
        if (nValidUrisFound)
            gotoSendCoinsPage();
        else if (nValidUrisFoundBit)
            gotoSendBitCoinsPage();
        else
            notificator->notify(Notificator::Warning, tr("URI处理"), tr("URI不能被解析！这可以通过一个无效的lecoin地址或畸形URI参数引起的."));
    }

    event->acceptProposedAction();
}

void BitcoinGUI::handleURI(QString strURI)
{
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
    }
    else if (sendBitCoinsPage->handleURI(strURI))
    {
        showNormalIfMinimized();
        gotoSendBitCoinsPage();
    }
    else
        notificator->notify(Notificator::Warning, tr("URI处理"), tr("URI不能被解析！这可以通过一个无效的LeCoin地址或畸形URI参数引起的."));
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        changePassphraseAction->setEnabled(false);
        logoutAction->setEnabled(false);
        lockWalletAction->setVisible(false);
        unlockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(true); // Testnet can startup unencrypted
        encryptWalletAction->setVisible(true);
        break;
    case WalletModel::Unlocked:
        changePassphraseAction->setEnabled(true);
        logoutAction->setEnabled(true);
        lockWalletAction->setEnabled(true);
        lockWalletAction->setVisible(true);
        unlockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        encryptWalletAction->setVisible(false);
        break;
    case WalletModel::Locked:
        changePassphraseAction->setEnabled(true);
        logoutAction->setEnabled(true);
        lockWalletAction->setVisible(false);
        unlockWalletAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        encryptWalletAction->setVisible(false);
        break;
    }
}

void BitcoinGUI::encryptWallet(bool status)
{
    if(!walletModel)
        return;

    if (fBootstrapTurbo)
    {
        QMessageBox::warning(this, tr("不允许"), tr("请等待，直到引导操作完成."));
        return;
    }

    fEncrypt = true;

    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt:
                                     AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

void BitcoinGUI::backupWallet()
{
    QString fileSeparator(QDir::separator());
    QString saveDir = GetDataDir().string().c_str();
    QFileDialog *dlg = new QFileDialog;
    QString filename = dlg->getSaveFileName(this, tr("备份钱包"), saveDir, tr("钱包数据 (*.dat)"));
    if(!filename.isEmpty())
    {
        if (!filename.endsWith(".dat"))
        {
            filename.append(".dat");
        }

#ifdef Q_OS_WIN
        // Qt in Windows stores the saved filename separators as "/",
        // so we need to change them back for comparison below.
        filename.replace(QRegExp("/"), fileSeparator);
#endif
        if ((filename.contains(saveDir) && filename.contains(fileSeparator + "wallet.dat")) ||
                filename.contains("blk0001.dat") ||
                filename.contains("bootstrap.") ||
                filename.contains("peers.dat") ||
                filename.length() < 5)
        {
            QMessageBox::warning(this, tr("备份不允许"), tr("请选择一个不同的名称为您的钱包备份\ n例如：钱包backup.dat"));
        }
        else if(!walletModel->backupWallet(filename))
        {
            QMessageBox::warning(this, tr("备份失败"), tr("有一个错误尝试钱包数据保存到新位置."));
        }
    }
    delete dlg;
}

void BitcoinGUI::changePassphrase()
{
    if (fBootstrapTurbo)
    {
        QMessageBox::warning(this, tr("不允许"), tr("请等待，直到引导操作完成."));
        return;
    }

    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::lockWallet()
{
    if (!walletModel)
        return;

    // Lock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Unlocked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Lock, this);
        dlg.setModel(walletModel);
        dlg.exec();
        stakingLabel->setText("权益中");
    }
}

void BitcoinGUI::unlockWallet()
{
    if (!walletModel)
        return;

    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
        stakingLabel->setText("权益中");
    }
}

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void BitcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcoinGUI::updateStakingIcon()
{
    if (!walletModel || pindexBest == pindexGenesisBlock)
        return;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(QDateTime::currentDateTime());
    int64_t currentBlock = clientModel->getNumBlocks();
    int peerBlock = clientModel->getNumBlocksOfPeers();
    if ((secs >= 90*60 && currentBlock < peerBlock) || !pwalletMain)
    {
        return;
    }
    uint64_t nWeight = 0;
    pwalletMain->GetStakeWeight(*pwalletMain, nWeight);
    progressBar->setVisible(false);
    overviewPage->showOutOfSyncWarning(false);
    double nNetworkWeight = GetPoSKernelPS();
    if (walletModel->getEncryptionStatus() == WalletModel::Unlocked && nLastCoinStakeSearchInterval && nWeight)
    {
        u_int64_t nEstimateTime = nTargetSpacing * nNetworkWeight / nWeight;
        QString text;
        if (nEstimateTime < 60)
        {
            text = tr("%n second(s)", "", nEstimateTime);
        }
        else if (nEstimateTime < 60*60)
        {
            text = tr("%n minute(s)", "", nEstimateTime/60);
        }
        else if (nEstimateTime < 24*60*60)
        {
            text = tr("%n hour(s)", "", nEstimateTime/(60*60));
        }
        else
        {
            text = tr("%n day(s)", "", nEstimateTime/(60*60*24));
        }
        stakingLabel->setText(QString("权益中"));
        labelBlocksIcon->hide();
        labelStakingIcon->show();
        int nStakeTimePower = pwalletMain->StakeTimeEarned(nWeight, pindexBest->pprev);
        if (PoSTprotocol(currentBlock) || fTestNet)
        {
            if (nStakeTimePower <= 100 && nStakeTimePower > 75)
            {
                labelStakingIcon->setPixmap(QIcon(":/icons/stake100").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
            }
            if (nStakeTimePower <= 75 && nStakeTimePower > 50)
            {
                labelStakingIcon->setPixmap(QIcon(":/icons/stake75").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
            }
            if (nStakeTimePower <= 50 && nStakeTimePower > 25)
            {
                labelStakingIcon->setPixmap(QIcon(":/icons/stake50").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
            }
            if (nStakeTimePower <= 25 && nStakeTimePower > 5)
            {
                labelStakingIcon->setPixmap(QIcon(":/icons/stake25").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
            }
            if (nStakeTimePower <= 5 && nStakeTimePower >= 0)
            {
                labelStakingIcon->setPixmap(QIcon(":/icons/stake0").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
            }
        }
        else
        {
            labelStakingIcon->setPixmap(QIcon(":/icons/staking_on").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        }
        if (PoSTprotocol(currentBlock) || fTestNet)
        {
            labelStakingIcon->setToolTip(tr("同步与权益累积中\n得到成熟的利益: %1%\n区块高度: %2\n预计多长时间赚取利息: %3").arg(nStakeTimePower).arg(currentBlock).arg(text));
        }
        else
        {
            labelStakingIcon->setToolTip(tr("同步与权益累积中\n区块高度: %1\n预计多长时间赚取利息: %2").arg(currentBlock).arg(text));
        }
    }
    else
    {
        stakingLabel->setText(QString("同步中"));
        labelBlocksIcon->hide();
        labelStakingIcon->show();
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_off").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        if (pwalletMain && pwalletMain->IsLocked())
            labelStakingIcon->setToolTip(tr("同步区块 %1\n关闭权益累积, 启动权益累积请在菜单中开启.").arg(currentBlock));
        else if (vNodes.empty())
            labelStakingIcon->setToolTip(tr("钱包离线啦，请确认是否在同步钱包,嘻嘻."));
        else
            labelStakingIcon->setToolTip(tr("同步区块 %1\n关闭权益累积, 得到权益累积时间.").arg(currentBlock));
    }
    // Update balance in balanceLabel
    setBalanceLabel(walletModel->getBalance(), walletModel->getStake(), walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance());
}

void BitcoinGUI::reloadBlockchainActionEnabled(bool enabled)
{
    reloadBlockchainAction->setEnabled(enabled);
}

void BitcoinGUI::reloadBlockchain(bool autoReload)
{
    boost::filesystem::path pathBootstrap(GetDataDir() / "bootstrap.zip");
    QUrl url(QString(walletDownloadsUrl).append("bootstrap.zip"));

    // Don't auto-bootstrap if the file has already been downloaded, unless the wallet is being encrypted.
    if (boost::filesystem::exists(pathBootstrap) && autoReload && !fEncrypt)
    {
        return;
    }

    // Don't allow multiple instances of bootstrapping
    reloadBlockchainActionEnabled(false); // Sets back to true when dialog closes.

    fBootstrapTurbo = true;

    printf("区块连数据下载中\n");
    Downloader *bs = new Downloader(this, walletModel);
    bs->setWindowTitle("重新扫描区块连数据");
    bs->setUrl(url);
    bs->setDest(boostPathToQString(pathBootstrap));
    bs->processBlockchain = true;
    if (autoReload) // Get bootsrap in auto mode (model)
    {
        bs->autoDownload = true;
        bs->exec();
        delete bs;
    }
    else
    {
        bs->show();
    }
}

void BitcoinGUI::rescanWallet()
{
    if (fBootstrapTurbo)
    {
        QMessageBox::warning(this, tr("不允许"), tr("请等待，直到引导操作完成."));
        return;
    }

    // No turning back. Ask permission.
    RescanDialog rs;
    rs.setModel(clientModel);
    rs.exec();
    if (!rs.rescanAccepted)
    {
        return;
    }
    fRescan = true;

    if (!walletModel->rescanWallet())
    {
        QMessageBox::warning(this, tr("重新扫描失败"), tr("有一个错误尝试重新扫描区块."));
    }
}

// Called by user
void BitcoinGUI::menuCheckForUpdate()
{
    if (fBootstrapTurbo)
    {
        QMessageBox::warning(this, tr("不允许"), tr("请等待，直到引导操作完成."));
        return;
    }

    fMenuCheckForUpdate = true;

    if (!fTimerCheckForUpdate)
        checkForUpdate();

    fMenuCheckForUpdate = false;
}

// Called by timer
void BitcoinGUI::timerCheckForUpdate()
{
    if (fBootstrapTurbo)
        return;

    if (fTimerCheckForUpdate)
        return;

    fTimerCheckForUpdate = true;

    if (!fMenuCheckForUpdate)
        checkForUpdate();

    fTimerCheckForUpdate = false;
}

void BitcoinGUI::checkForUpdateActionEnabled(bool enabled)
{
    checkForUpdateAction->setEnabled(enabled);
}

void BitcoinGUI::checkForUpdate()
{
    boost::filesystem::path fileName(GetDataDir());
    QUrl url;

    if (fMenuCheckForUpdate)
        fNewVersion = false; // Force a reload of the version file if the user requested a check and a new version was already found

    ReadVersionFile();

    // Set version icon good/bad
    setVersionIcon(fNewVersion);

    if (fNewVersion)
    {
        // No turning back. Ask permission.
        UpdateDialog ud;
        ud.setModel(clientModel);
        ud.exec();
        if (!ud.updateAccepted)
        {
            return;
        }

        checkForUpdateActionEnabled(false); // Sets back to true when dialog closes.

        std::string basename = GetArg("-vFileName","lecoin-qt");
        fileName = fileName / basename.c_str();
        url.setUrl(QString(walletDownloadsUrl).append(basename.c_str()));

        printf("新钱包下载中\n");
        Downloader *w = new Downloader(this, walletModel);
        w->setWindowTitle("钱包下载");
        w->setUrl(url);
        w->setDest(boostPathToQString(fileName));
        w->autoDownload = false;
        w->processUpdate = true;
        w->show();
    }
    else
    {
        if (fMenuCheckForUpdate)
        {
            // No update required, show what's new.
            WhatsNewDialog wn;
            wn.setModel(clientModel);
            wn.exec();
        }
    }
}
