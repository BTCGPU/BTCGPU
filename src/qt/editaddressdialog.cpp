// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/editaddressdialog.h>
#include <qt/forms/ui_editaddressdialog.h>

#include <qt/addresstablemodel.h>
#include <qt/guiutil.h>

#include <QDataWidgetMapper>
#include <QMessageBox>

extern OutputType g_address_type;

EditAddressDialog::EditAddressDialog(Mode _mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditAddressDialog),
    mapper(0),
    mode(_mode),
    model(0)
{
    ui->setupUi(this);

    GUIUtil::setupAddressWidget(ui->addressEdit, this);

    switch(mode)
    {
    case NewReceivingAddress:
        setWindowTitle(tr("New receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case NewSendingAddress:
        setWindowTitle(tr("New sending address"));
        break;
    case EditReceivingAddress:
        setWindowTitle(tr("Edit receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case EditSendingAddress:
        setWindowTitle(tr("Edit sending address"));
        break;
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
}

EditAddressDialog::~EditAddressDialog()
{
    delete ui;
}

void EditAddressDialog::setModel(AddressTableModel *_model)
{
    this->model = _model;
    if(!_model)
        return;

    mapper->setModel(_model);
    mapper->addMapping(ui->labelEdit, AddressTableModel::Label);
    mapper->addMapping(ui->addressEdit, AddressTableModel::Address);
}

void EditAddressDialog::loadRow(int row)
{
    mapper->setCurrentIndex(row);
}

bool EditAddressDialog::saveCurrentRow()
{
    if(!model)
        return false;

    switch(mode)
    {
    case NewReceivingAddress:
    case NewSendingAddress:
        address = model->addRow(
                mode == NewSendingAddress ? AddressTableModel::Send : AddressTableModel::Receive,
                ui->labelEdit->text(),
                ui->addressEdit->text(),
                g_address_type);
        break;
    case EditReceivingAddress:
    case EditSendingAddress:
        if(mapper->submit())
        {
            address = ui->addressEdit->text();
        }
        break;
    }
    return !address.isEmpty();
}

void EditAddressDialog::accept()
{
    if(!model)
        return;

    if(!saveCurrentRow())
    {
        switch(model->getEditStatus())
        {
        case AddressTableModel::OK:
            // Failed with unknown reason. Just reject.
            break;
        case AddressTableModel::NO_CHANGES:
            // No changes were made during edit operation. Just reject.
            break;
        case AddressTableModel::INVALID_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is not a valid Bitcoin address.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::DUPLICATE_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is already in the address book.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::WALLET_UNLOCK_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("Could not unlock wallet."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::KEY_GENERATION_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("New key generation failed."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;

        }
        return;
    }
    QDialog::accept();
}

QString EditAddressDialog::getAddress() const
{
    return address;
}

void EditAddressDialog::setAddress(const QString &_address)
{
    this->address = _address;
    ui->addressEdit->setText(_address);
}
