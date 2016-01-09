#include "askpassphrasedialog.h"
#include "ui_askpassphrasedialog.h"
#include "init.h"
#include "util.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "walletmodel.h"

#include <QMessageBox>
#include <QPushButton>
#include <QKeyEvent>

using namespace GUIUtil;

AskPassphraseDialog::AskPassphraseDialog(Mode mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AskPassphraseDialog),
    mode(mode),
    model(0),
    fCapsLock(false)
{
    ui->setupUi(this);

    ui->passEdit1->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit2->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit3->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    // Setup Caps Lock detection.
    ui->passEdit1->installEventFilter(this);
    ui->passEdit2->installEventFilter(this);
    ui->passEdit3->installEventFilter(this);

    switch(mode)
    {
        case Encrypt: // Ask passphrase x2
            ui->passLabel1->hide();
            ui->passEdit1->hide();
            ui->warningLabel->setText(tr("钱包输入新密码.<br/>请使用密码 <b>10个或更多随机字符</b>, or <b>八个或更多的话</b>."));
            setWindowTitle(tr("加密钱包..."));
            break;
        case Lock: // Ask passphrase
            ui->warningLabel->setText(tr("单击确定关闭POS."));
            ui->passLabel1->hide();
            ui->passEdit1->hide();
            ui->passLabel2->hide();
            ui->passEdit2->hide();
            ui->passLabel3->hide();
            ui->passEdit3->hide();
            setWindowTitle(tr("POS 关闭"));
            break;
        case Unlock: // Ask passphrase
            ui->warningLabel->setText(tr("输入你的钱包密码."));
            ui->passLabel2->hide();
            ui->passEdit2->hide();
            ui->passLabel3->hide();
            ui->passEdit3->hide();
            setWindowTitle(tr("输入密码"));
            break;
        case Decrypt:   // Ask passphrase
            ui->warningLabel->setText(tr("输入你的钱包密码"));
            ui->passLabel2->hide();
            ui->passEdit2->hide();
            ui->passLabel3->hide();
            ui->passEdit3->hide();
            setWindowTitle(tr("解密钱包"));
            break;
        case ChangePass: // Ask old passphrase + new passphrase x2
            ui->warningLabel->setText(tr("更改钱包密码."));
            setWindowTitle(tr("更改口令"));
            break;
    }

    connect(ui->passEdit1, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->passEdit2, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->passEdit3, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
}

AskPassphraseDialog::~AskPassphraseDialog()
{
    // Attempt to overwrite text so that they do not linger around in memory
    ui->passEdit1->setText(QString(" ").repeated(ui->passEdit1->text().size()));
    ui->passEdit2->setText(QString(" ").repeated(ui->passEdit2->text().size()));
    ui->passEdit3->setText(QString(" ").repeated(ui->passEdit3->text().size()));
    delete ui;
}

void AskPassphraseDialog::setModel(WalletModel *model)
{
    this->model = model;

    switch(mode)
    {
        case Encrypt: // Ask passphrase x2
            break;
        case Lock: // Ask passphrase
            ui->passEdit1->setText("password"); // Real password not required to Lock.
            break;
        case Unlock: // Ask passphrase
            break;
        case Decrypt:   // Ask passphrase
            break;
        case ChangePass: // Ask old passphrase + new passphrase x2
            break;
    }
}

void AskPassphraseDialog::accept()
{
    SecureString oldpass, newpass1, newpass2;
    if(!model)
        return;
    oldpass.reserve(MAX_PASSPHRASE_SIZE);
    newpass1.reserve(MAX_PASSPHRASE_SIZE);
    newpass2.reserve(MAX_PASSPHRASE_SIZE);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make this input mlock()'d to begin with.
    oldpass.assign(ui->passEdit1->text().toStdString().c_str());
    newpass1.assign(ui->passEdit2->text().toStdString().c_str());
    newpass2.assign(ui->passEdit3->text().toStdString().c_str());

    switch(mode)
    {
    case Encrypt: {
        if(newpass1.empty() || newpass2.empty())
        {
            // Cannot encrypt with empty passphrase
            break;
        }
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("确认钱包加密"),
                 tr("警告：如果你加密你的钱包，并失去了你的密码，您将</b>失去所有的硬币!") + "<br><br>" + tr("你确定你想你的钱包加密？?"),
                 QMessageBox::Yes|QMessageBox::Cancel,
                 QMessageBox::Cancel);
        if(retval == QMessageBox::Yes)
        {
            if(newpass1 == newpass2)
            {
                if(model->setWalletEncrypted(true, newpass1))
                {
                    QMessageBox::warning(this, tr("钱包加密"),
                                         "<qt>" + 
                                         tr("LeCoin现在将重新启动以完成加密过程. "
                                         "请记住，加密你的钱包不能完全保护 "
                                         "从你的硬币感染您的计算机被窃取恶意软件.") +
                                         "<br><br><b>" + 
                                         tr("重要提示：你取得了你的钱包文件的所有以前的备份 "
                                         "应替换新生成的，加密的钱包文件. "
                                         "出于安全原因，未加密的钱夹文件的以前的备份 "
                                         "将成为无用的，一旦你开始使用新的加密钱包.") +
                                         "</b></qt>");
                    MilliSleep(1 * 1000);
                    fRestart = true;
                    StartShutdown();
                }
                else
                {
                    QMessageBox::critical(this, tr("钱包加密失败"),
                                         tr("钱包加密失败，原因是内部错误。你的钱包没有加密."));
                }
                QDialog::accept(); // Success
            }
            else
            {
                QMessageBox::critical(this, tr("钱包加密失败"),
                                     tr("所提供的密码不匹配."));
            }
        }
        else
        {
            QDialog::reject(); // Cancelled
        }
        } break;
    case Lock:
        if(!model->setWalletLocked(true, oldpass))
        {
            QMessageBox::critical(this, tr("钱包加密失败"),
                                  tr("钱包输入的密码不正确."));
        }
        else
        {
            QDialog::accept(); // Success
        }
        break;
    case Unlock:
        if(!model->setWalletLocked(false, oldpass))
        {
            QMessageBox::critical(this, tr("钱包解锁失败"),
                                  tr("钱包输入的密码不正确."));
        }
        else
        {
            QDialog::accept(); // Success
        }
        break;
    case Decrypt:
        if(!model->setWalletEncrypted(false, oldpass))
        {
            QMessageBox::critical(this, tr("钱包解密失败"),
                                  tr("钱包解密输入的密码不正确."));
        }
        else
        {
            QDialog::accept(); // Success
        }
        break;
    case ChangePass:
        if(newpass1 == newpass2)
        {
            if(model->changePassphrase(oldpass, newpass1))
            {
                QMessageBox::information(this, tr("钱包加密"),
                                     tr("钱包密码更改成功."));
                QDialog::accept(); // Success
            }
            else
            {
                QMessageBox::critical(this, tr("钱包加密失败"),
                                     tr("钱包输入的密码不正确."));
            }
        }
        else
        {
            QMessageBox::critical(this, tr("钱包加密失败"),
                                 tr("所提供的口令不匹配."));
        }
        break;
    }
}

void AskPassphraseDialog::textChanged()
{
    // Validate input, set Ok button to enabled when acceptable
    bool acceptable = false;
    switch(mode)
    {
    case Encrypt: // New passphrase x2
        acceptable = !ui->passEdit2->text().isEmpty() && !ui->passEdit3->text().isEmpty();
        break;
    case Lock: // Old passphrase x1
        acceptable = true;
        break;
    case Unlock: // Old passphrase x1
        acceptable = true;
        break;
    case Decrypt:
        acceptable = !ui->passEdit1->text().isEmpty();
        break;
    case ChangePass: // Old passphrase x1, new passphrase x2
        acceptable = !ui->passEdit1->text().isEmpty() && !ui->passEdit2->text().isEmpty() && !ui->passEdit3->text().isEmpty();
        break;
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
}

bool AskPassphraseDialog::event(QEvent *event)
{
    // Detect Caps Lock key press.
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_CapsLock) {
            fCapsLock = !fCapsLock;
        }
        if (fCapsLock) {
            ui->capsLabel->setText(tr("警告：大写建打开!"));
        } else {
            ui->capsLabel->clear();
        }
    }
    return QWidget::event(event);
}

bool AskPassphraseDialog::eventFilter(QObject *object, QEvent *event)
{
    /* Detect Caps Lock.
     * There is no good OS-independent way to check a key state in Qt, but we
     * can detect Caps Lock by checking for the following condition:
     * Shift key is down and the result is a lower case character, or
     * Shift key is not down and the result is an upper case character.
     */
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        QString str = ke->text();
        if (str.length() != 0) {
            const QChar *psz = str.unicode();
            bool fShift = (ke->modifiers() & Qt::ShiftModifier) != 0;
            if ((fShift && psz->isLower()) || (!fShift && psz->isUpper())) {
                fCapsLock = true;
                ui->capsLabel->setText(tr("警告：大写建打开!"));
            } else if (psz->isLetter()) {
                fCapsLock = false;
                ui->capsLabel->clear();
            }
        }
    }
    return QDialog::eventFilter(object, event);
}
