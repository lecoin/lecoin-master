#include "rescandialog.h"
#include "ui_rescandialog.h"
#include "clientmodel.h"
#include "util.h"
#include "guiutil.h"
#include "guiconstants.h"

using namespace GUIUtil;

bool rescanAccepted = false;

RescanDialog::RescanDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RescanDialog)
{
    ui->setupUi(this);

    ui->statusLabel->setFont(qFont);
    ui->statusLabel->setText("请确认重新扫描区块到你的钱包的交易。这个过程可能需要多达10至20分钟才能完成，您的钱包将重新启动，开始扫描.");
}

void RescanDialog::setModel(ClientModel *model)
{
    if(model)
    {
    }
}

RescanDialog::~RescanDialog()
{
    delete ui;
}

void RescanDialog::on_buttonBox_accepted()
{
    rescanAccepted = true;
    close();
}

void RescanDialog::on_buttonBox_rejected()
{
    rescanAccepted = false;
    close();
}
