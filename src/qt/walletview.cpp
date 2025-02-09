// Copyright (c) 2011-2015 The Bitcoin developers
// Copyright (c) 2016-2018 The AGORA developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletview.h"

#include "addressbookpage.h"
#include "bip38tooldialog.h"
#include "bitcoingui.h"
#include "blockexplorer.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "masternodeconfig.h"
#include "multisenddialog.h"
#include "multisigdialog.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "receivecoinsdialog.h"
#include "privacydialog.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "transactiontablemodel.h"
#include "transactionview.h"
#include "walletmodel.h"

#include "ui_interface.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>

WalletView::WalletView(QWidget* parent) : QStackedWidget(parent),
clientModel(0),
walletModel(0)
{
	// Create tabs
	overviewPage = new OverviewPage();
	explorerWindow = new BlockExplorer(this);
	transactionsPage = new QWidget(this);

	QVBoxLayout* vbox = new QVBoxLayout();
	vbox->setContentsMargins(QMargins());
	vbox->setSpacing(0);

	transactionView = new TransactionView(this);
	vbox->addWidget(transactionView);

	QHBoxLayout* hbox_buttons = new QHBoxLayout();
	hbox_buttons->addStretch();

	// Sum of selected transactions
	QLabel* transactionSumLabel = new QLabel();                // Label
	transactionSumLabel->setObjectName("transactionSumLabel"); // Label ID as CSS-reference
	transactionSumLabel->setText(tr("Selected amount:"));
	hbox_buttons->addWidget(transactionSumLabel);

	transactionSum = new QLabel();                   // Amount
	transactionSum->setObjectName("transactionSum"); // Label ID as CSS-reference
	transactionSum->setMinimumSize(200, 8);
	transactionSum->setTextInteractionFlags(Qt::TextSelectableByMouse);
	hbox_buttons->addWidget(transactionSum);

	QPushButton* exportButton = new QPushButton(tr("&Export"), this);
	exportButton->setToolTip(tr("Export the data in the current tab to a file"));
	hbox_buttons->addWidget(exportButton);

	hbox_buttons->addStretch();
	hbox_buttons->setContentsMargins(0, 20, 0, 0);

	vbox->addLayout(hbox_buttons);

	QFrame* outer_frame = new QFrame;
	outer_frame->setLayout(vbox);
	outer_frame->setObjectName("transactions_page");

	int ds_blur = 70;
	int ds_yoff = 15;
	QGraphicsDropShadowEffect* drop_shadow_outer_frame = new QGraphicsDropShadowEffect;
	drop_shadow_outer_frame->setBlurRadius(ds_blur);
	drop_shadow_outer_frame->setXOffset(0);
	drop_shadow_outer_frame->setYOffset(ds_yoff);
	drop_shadow_outer_frame->setColor(QColor(59, 76, 107, 50));
	outer_frame->setGraphicsEffect(drop_shadow_outer_frame);

	QHBoxLayout* outer_layout = new QHBoxLayout;
	outer_layout->addWidget(outer_frame);
	outer_layout->setContentsMargins(35, 42, 35, 42);
	outer_layout->setSpacing(0);

	transactionsPage->setLayout(outer_layout);
	privacyPage = new PrivacyDialog();
	receiveCoinsPage = new ReceiveCoinsDialog();
	sendCoinsPage = new SendCoinsDialog();

	addWidget(overviewPage);
	addWidget(transactionsPage);
	addWidget(privacyPage);
	addWidget(receiveCoinsPage);
	addWidget(sendCoinsPage);
	addWidget(explorerWindow);

	QSettings settings;
	if (settings.value("fShowMasternodesTab").toBool()) {
		masternodeListPage = new MasternodeList();
		addWidget(masternodeListPage);
	}

	// Clicking on a transaction on the overview pre-selects the transaction on the transaction history page
	connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

	// Double-clicking on a transaction on the transaction history page shows details
	connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

	// Update wallet with sum of selected transactions
	connect(transactionView, SIGNAL(trxAmount(QString)), this, SLOT(trxAmount(QString)));

	// Clicking on "Export" allows to export the transaction list
	connect(exportButton, SIGNAL(clicked()), transactionView, SLOT(exportClicked()));

	// Pass through messages from sendCoinsPage
	connect(sendCoinsPage, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

	// Pass through messages from transactionView
	connect(transactionView, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));
}

WalletView::~WalletView()
{
}

void WalletView::setBitcoinGUI(BitcoinGUI* gui)
{
	if (gui) {
		// Clicking on a transaction on the overview page simply sends you to transaction history page
		connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), gui, SLOT(gotoHistoryPage()));

		// Receive and report messages
		connect(this, SIGNAL(message(QString, QString, unsigned int)), gui, SLOT(message(QString, QString, unsigned int)));

		// Pass through encryption status changed signals
		connect(this, SIGNAL(encryptionStatusChanged(int)), gui, SLOT(setEncryptionStatus(int)));

		// Pass through transaction notifications
		connect(this, SIGNAL(incomingTransaction(QString, int, CAmount, QString, QString)), gui, SLOT(incomingTransaction(QString, int, CAmount, QString, QString)));
	}
}

void WalletView::setClientModel(ClientModel* clientModel)
{
	this->clientModel = clientModel;

	overviewPage->setClientModel(clientModel);
	sendCoinsPage->setClientModel(clientModel);
	QSettings settings;
	if (settings.value("fShowMasternodesTab").toBool()) {
		masternodeListPage->setClientModel(clientModel);
	}
}

void WalletView::setWalletModel(WalletModel* walletModel)
{
	this->walletModel = walletModel;

	// Put transaction list in tabs
	transactionView->setModel(walletModel);
	overviewPage->setWalletModel(walletModel);
	QSettings settings;
	if (settings.value("fShowMasternodesTab").toBool()) {
		masternodeListPage->setWalletModel(walletModel);
	}
	privacyPage->setModel(walletModel);
	receiveCoinsPage->setModel(walletModel);
	sendCoinsPage->setModel(walletModel);

	if (walletModel) {
		// Receive and pass through messages from wallet model
		connect(walletModel, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

		// Handle changes in encryption status
		connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SIGNAL(encryptionStatusChanged(int)));
		updateEncryptionStatus();

		// Balloon pop-up for new transaction
		connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex, int, int)),
			this, SLOT(processNewTransaction(QModelIndex, int, int)));

		// Ask for passphrase if needed
		connect(walletModel, SIGNAL(requireUnlock(AskPassphraseDialog::Context)), this, SLOT(unlockWallet(AskPassphraseDialog::Context)));

		// Show progress dialog
		connect(walletModel, SIGNAL(showProgress(QString, int)), this, SLOT(showProgress(QString, int)));
	}
}

void WalletView::processNewTransaction(const QModelIndex& parent, int start, int /*end*/)
{
	// Prevent balloon-spam when initial block download is in progress
	if (!walletModel || !clientModel || clientModel->inInitialBlockDownload())
		return;

	TransactionTableModel* ttm = walletModel->getTransactionTableModel();
	if (!ttm || ttm->processingQueuedTransactions())
		return;

	QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
	qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
	QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
	QString address = ttm->index(start, TransactionTableModel::ToAddress, parent).data().toString();

	emit incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address);
}

void WalletView::gotoOverviewPage()
{
	setCurrentWidget(overviewPage);
	// Refresh UI-elements in case coins were locked/unlocked in CoinControl
	walletModel->emitBalanceChanged();
}

void WalletView::gotoHistoryPage()
{
	setCurrentWidget(transactionsPage);
}


void WalletView::gotoBlockExplorerPage()
{
	setCurrentWidget(explorerWindow);
}

void WalletView::gotoMasternodePage()
{
	QSettings settings;
	if (settings.value("fShowMasternodesTab").toBool()) {
		setCurrentWidget(masternodeListPage);
	}
}

void WalletView::gotoReceiveCoinsPage()
{
	setCurrentWidget(receiveCoinsPage);
}

void WalletView::gotoPrivacyPage()
{
	setCurrentWidget(privacyPage);
	// Refresh UI-elements in case coins were locked/unlocked in CoinControl
	walletModel->emitBalanceChanged();
}

void WalletView::gotoSendCoinsPage(QString addr)
{
	setCurrentWidget(sendCoinsPage);

	if (!addr.isEmpty())
		sendCoinsPage->setAddress(addr);
}

void WalletView::gotoSignMessageTab(QString addr)
{
	// calls show() in showTab_SM()
	SignVerifyMessageDialog* signVerifyMessageDialog = new SignVerifyMessageDialog(this);
	signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
	signVerifyMessageDialog->setModel(walletModel);
	signVerifyMessageDialog->showTab_SM(true);

	if (!addr.isEmpty())
		signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
	// calls show() in showTab_VM()
	SignVerifyMessageDialog* signVerifyMessageDialog = new SignVerifyMessageDialog(this);
	signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
	signVerifyMessageDialog->setModel(walletModel);
	signVerifyMessageDialog->showTab_VM(true);

	if (!addr.isEmpty())
		signVerifyMessageDialog->setAddress_VM(addr);
}

void WalletView::gotoBip38Tool()
{
	Bip38ToolDialog* bip38ToolDialog = new Bip38ToolDialog(this);
	//bip38ToolDialog->setAttribute(Qt::WA_DeleteOnClose);
	bip38ToolDialog->setModel(walletModel);
	bip38ToolDialog->showTab_ENC(true);
}

void WalletView::gotoMultiSendDialog()
{
	MultiSendDialog* multiSendDialog = new MultiSendDialog(this);
	multiSendDialog->setModel(walletModel);
	multiSendDialog->show();
}

void WalletView::gotoMultisigDialog(int index)
{
	MultisigDialog* multisig = new MultisigDialog(this);
	multisig->setModel(walletModel);
	multisig->showTab(index);
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
	return sendCoinsPage->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
	overviewPage->showOutOfSyncWarning(fShow);
	privacyPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus()
{
	emit encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void WalletView::encryptWallet(bool status)
{
	if (!walletModel)
		return;
	AskPassphraseDialog dlg(status ? AskPassphraseDialog::Mode::Encrypt : AskPassphraseDialog::Mode::Decrypt, this,
		walletModel, AskPassphraseDialog::Context::Encrypt);
	dlg.exec();

	updateEncryptionStatus();
}

void WalletView::backupWallet()
{
	QString filename = GUIUtil::getSaveFileName(this,
		tr("Backup Wallet"), QString(),
		tr("Wallet Data (*.dat)"), NULL);

	if (filename.isEmpty())
		return;
	walletModel->backupWallet(filename);
}

void WalletView::changePassphrase()
{
	AskPassphraseDialog dlg(AskPassphraseDialog::Mode::ChangePass, this, walletModel, AskPassphraseDialog::Context::ChangePass);
	dlg.exec();
}

void WalletView::unlockWallet(AskPassphraseDialog::Context context)
{
	if (!walletModel)
		return;
	// Unlock wallet when requested by wallet model

	if (walletModel->getEncryptionStatus() == WalletModel::Locked || walletModel->getEncryptionStatus() == WalletModel::UnlockedForAnonymizationOnly) {
		AskPassphraseDialog dlg(AskPassphraseDialog::Mode::UnlockAnonymize, this, walletModel, context);
		dlg.exec();
	}
}

void WalletView::lockWallet()
{
	if (!walletModel)
		return;

	walletModel->setWalletLocked(true);
}

void WalletView::toggleLockWallet()
{
	if (!walletModel)
		return;

	WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

	// Unlock the wallet when requested
	if (encStatus == walletModel->Locked) {
		AskPassphraseDialog dlg(AskPassphraseDialog::Mode::UnlockAnonymize, this, walletModel, AskPassphraseDialog::Context::ToggleLock);
		dlg.exec();
	}

	else if (encStatus == walletModel->Unlocked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
		walletModel->setWalletLocked(true);
	}
}

void WalletView::usedSendingAddresses()
{
	if (!walletModel)
		return;
	AddressBookPage* dlg = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setModel(walletModel->getAddressTableModel());
	dlg->show();
}

void WalletView::usedReceivingAddresses()
{
	if (!walletModel)
		return;
	AddressBookPage* dlg = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setModel(walletModel->getAddressTableModel());
	dlg->show();
}

void WalletView::showProgress(const QString& title, int nProgress)
{
	if (nProgress == 0) {
		progressDialog = new QProgressDialog(title, "", 0, 100);
		progressDialog->setWindowModality(Qt::ApplicationModal);
		progressDialog->setMinimumDuration(0);
		progressDialog->setCancelButton(0);
		progressDialog->setAutoClose(false);
		progressDialog->setValue(0);
	}
	else if (nProgress == 100) {
		if (progressDialog) {
			progressDialog->close();
			progressDialog->deleteLater();
		}
	}
	else if (progressDialog)
		progressDialog->setValue(nProgress);
}

/** Update wallet with the sum of the selected transactions */
void WalletView::trxAmount(QString amount)
{
	transactionSum->setText(amount);
}
