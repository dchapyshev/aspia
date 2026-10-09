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

#include "client/desktop/credentials/credential_panel.h"

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QToolButton>

#include "base/logging.h"
#include "client/database.h"

namespace {

const int kMargin = 16;
const qreal kCornerRadius = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
CredentialPanel::CredentialPanel(QWidget* parent)
    : QWidget(parent)
{
    CHECK(parent);

    combo_credential_ = new QComboBox(this);
    combo_credential_->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    QToolButton* sign_in_button = new QToolButton(this);
    sign_in_button->setIcon(QIcon(":/img/login.svg"));
    sign_in_button->setIconSize(QSize(24, 24));
    sign_in_button->setToolTip(tr("Sign In"));
    sign_in_button->setAutoRaise(true);
    connect(sign_in_button, &QToolButton::clicked, this, &CredentialPanel::onSignIn);

    QToolButton* close_button = new QToolButton(this);
    close_button->setIcon(QIcon(":/img/close.svg"));
    close_button->setIconSize(QSize(24, 24));
    close_button->setToolTip(tr("Close"));
    close_button->setAutoRaise(true);
    connect(close_button, &QToolButton::clicked, this, &CredentialPanel::onDismiss);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(combo_credential_);
    layout->addWidget(sign_in_button);
    layout->addWidget(close_button);

    parent->installEventFilter(this);
    hide();
}

//--------------------------------------------------------------------------------------------------
CredentialPanel::~CredentialPanel() = default;

//--------------------------------------------------------------------------------------------------
void CredentialPanel::popup()
{
    QList<CredentialConfig> credentials;
    if (Database::instance().credentialList(&credentials, CredentialConfig::Type::OS) != Database::ReadResult::OK)
        LOG(ERROR) << "Unable to read credentials";

    credentials_.clear();
    combo_credential_->clear();

    const QIcon icon(":/img/keys.svg");

    for (const CredentialConfig& credential : std::as_const(credentials))
    {
        if (!credential.isValid())
            continue;

        combo_credential_->addItem(icon, credential.displayName());
        credentials_.append(credential);
    }

    if (credentials_.isEmpty())
    {
        onDismiss();
        return;
    }

    adjustSize();
    updatePosition();
    show();
    raise();
}

//--------------------------------------------------------------------------------------------------
void CredentialPanel::onDismiss()
{
    const bool had_focus = isAncestorOf(QApplication::focusWidget());

    hide();
    credentials_.clear();
    combo_credential_->clear();

    if (had_focus)
        parentWidget()->setFocus();
}

//--------------------------------------------------------------------------------------------------
bool CredentialPanel::eventFilter(QObject* object, QEvent* event)
{
    if (object == parentWidget() && event->type() == QEvent::Resize)
        updatePosition();

    return QWidget::eventFilter(object, event);
}

//--------------------------------------------------------------------------------------------------
void CredentialPanel::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(palette().brush(QPalette::Window));
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kCornerRadius, kCornerRadius);
}

//--------------------------------------------------------------------------------------------------
void CredentialPanel::onSignIn()
{
    const int index = combo_credential_->currentIndex();
    if (index < 0 || index >= credentials_.size())
        return;

    const CredentialConfig credential = credentials_[index];

    LOG(INFO) << "[ACTION] Sign in with saved credentials";
    onDismiss();

    emit sig_credentialSelected(credential.username(), credential.password());
}

//--------------------------------------------------------------------------------------------------
void CredentialPanel::updatePosition()
{
    const QRect parent_rect = parentWidget()->rect();
    move(parent_rect.center().x() - width() / 2, parent_rect.bottom() - height() - kMargin);
}
