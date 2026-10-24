//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/android/host_key_dialog.h"

#include <QFontDatabase>
#include <QVBoxLayout>

#include "base/logging.h"
#include "base/peer/host_key.h"
#include "client/database.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/message_dialog.h"

//--------------------------------------------------------------------------------------------------
HostKeyDialog::HostKeyDialog(const QString& address, const QByteArray& public_key, bool key_changed,
                             QWidget* parent)
    : Dialog(parent)
{
    setTitle(tr("Host Identity"));

    if (key_changed)
    {
        setText(tr("WARNING: THE IDENTITY KEY OF HOST %1 HAS CHANGED!\n\n"
                   "Someone may be intercepting the connection to steal your password, or the host "
                   "was reinstalled. Do not continue unless the administrator of the host has "
                   "confirmed the new fingerprint.").arg(address));
    }
    else
    {
        setText(tr("You are connecting to host %1 with an operating system / domain account for "
                   "the first time. Make sure that the fingerprint below matches the one shown in "
                   "the host settings (Domain tab) before you send your password.").arg(address));
    }

    Label* caption = new Label(tr("Fingerprint (SHA-256):"), Label::Role::CAPTION, this);
    caption->setWordWrap(true);

    Label* fingerprint = new Label(HostKey::fingerprint(public_key), Label::Role::BODY, this);
    fingerprint->setWordWrap(true);
    fingerprint->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    fingerprint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (key_changed)
        fingerprint->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));

    Label* question = new Label(tr("Do you trust this host?"), Label::Role::BODY, this);
    question->setWordWrap(true);

    contentLayout()->addWidget(caption);
    contentLayout()->addWidget(fingerprint);
    contentLayout()->addWidget(question);

    // A changed key is never the prominent action.
    Button* reject = addButton(tr("Reject"), key_changed ? Button::Role::FILLED : Button::Role::TEXT);
    Button* trust = addButton(key_changed ? tr("Trust the new key") : tr("Trust"),
                              key_changed ? Button::Role::TEXT : Button::Role::FILLED);

    connect(reject, &Button::clicked, this, &QDialog::reject);
    connect(trust, &Button::clicked, this, &QDialog::accept);
}

//--------------------------------------------------------------------------------------------------
HostKeyDialog::~HostKeyDialog() = default;

//--------------------------------------------------------------------------------------------------
// static
bool HostKeyDialog::confirm(QWidget* parent, const QString& address, const QByteArray& public_key,
                            bool key_changed)
{
    if (public_key.isEmpty() || address.isEmpty() || HostKey::fingerprint(public_key).isEmpty())
    {
        LOG(ERROR) << "Invalid host key or address";
        return false;
    }

    LOG(INFO) << "Host key confirmation (address:" << address << "changed:" << key_changed
              << "fingerprint:" << HostKey::fingerprint(public_key) << ")";

    HostKeyDialog dialog(address, public_key, key_changed, parent);
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(INFO) << "[ACTION] Host key rejected by user";
        return false;
    }

    LOG(INFO) << "[ACTION] Host key accepted by user";

    if (!Database::instance().setHostPublicKey(address, public_key))
    {
        LOG(ERROR) << "Unable to save the host key";
        MessageDialog::info(parent, tr("Host Identity"),
                            tr("Unable to save the identity key of the host."));
        return false;
    }

    return true;
}
