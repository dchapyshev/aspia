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
#include <QLabel>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

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

    label_message_ = new QLabel(this);
    label_message_->setAlignment(Qt::AlignCenter);
    label_message_->setStyleSheet("color: #e5484d;");
    label_message_->hide();

    combo_credential_ = new QComboBox(this);
    combo_credential_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(combo_credential_, &QComboBox::currentIndexChanged, this, &CredentialPanel::onCurrentCredentialChanged);

    sign_in_button_ = new QToolButton(this);
    sign_in_button_->setIcon(QIcon(":/img/login.svg"));
    sign_in_button_->setIconSize(QSize(24, 24));
    sign_in_button_->setToolTip(tr("Sign In"));
    sign_in_button_->setAutoRaise(true);
    connect(sign_in_button_, &QToolButton::clicked, this, &CredentialPanel::onSignIn);

    QToolButton* close_button = new QToolButton(this);
    close_button->setIcon(QIcon(":/img/close.svg"));
    close_button->setIconSize(QSize(24, 24));
    close_button->setToolTip(tr("Close"));
    close_button->setAutoRaise(true);
    connect(close_button, &QToolButton::clicked, this, &CredentialPanel::onDismiss);

    QHBoxLayout* controls_layout = new QHBoxLayout();
    controls_layout->addWidget(combo_credential_);
    controls_layout->addWidget(sign_in_button_);
    controls_layout->addWidget(close_button);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(label_message_);
    layout->addLayout(controls_layout);

    parent->installEventFilter(this);
    hide();
}

//--------------------------------------------------------------------------------------------------
CredentialPanel::~CredentialPanel() = default;

//--------------------------------------------------------------------------------------------------
void CredentialPanel::popup(qint64 credential_id, const QString& message)
{
    QList<CredentialConfig> credentials;
    if (Database::instance().credentialList(&credentials, CredentialConfig::Type::OS) != Database::ReadResult::OK)
        LOG(ERROR) << "Unable to read credentials";

    credentials_.clear();
    combo_credential_->clear();

    int current = -1;

    for (const CredentialConfig& credential : std::as_const(credentials))
    {
        if (!credential.isValid())
            continue;

        if (credential.id() == credential_id)
            current = credentials_.size();

        credentials_.append(credential);
    }

    if (credentials_.isEmpty())
    {
        onDismiss();
        return;
    }

    const QIcon icon(":/img/keys.svg");

    if (current < 0)
    {
        const QPixmap disabled = icon.pixmap(combo_credential_->iconSize(), devicePixelRatioF(), QIcon::Disabled);
        combo_credential_->addItem(QIcon(disabled), tr("Not Selected"));
    }

    for (int i = 0; i < credentials_.size(); ++i)
        combo_credential_->addItem(icon, credentials_[i].displayName(), i);

    if (current >= 0)
        combo_credential_->setCurrentIndex(combo_credential_->findData(current));

    onCurrentCredentialChanged();

    label_message_->setText(message);
    label_message_->setVisible(!message.isEmpty());

    adjustSize();
    updatePosition();
    show();
    raise();
}

//--------------------------------------------------------------------------------------------------
bool CredentialPanel::signIn()
{
    const QVariant data = combo_credential_->currentData();
    if (!data.isValid())
        return false;

    const int index = data.toInt();
    if (index < 0 || index >= credentials_.size())
        return false;

    const CredentialConfig credential = credentials_[index];

    LOG(INFO) << "[ACTION] Sign in with saved credentials";
    onDismiss();

    emit sig_credentialSelected(credential.id(), credential.username(), credential.password());
    return true;
}

//--------------------------------------------------------------------------------------------------
void CredentialPanel::onDismiss()
{
    const bool had_focus = isAncestorOf(QApplication::focusWidget());

    hide();
    credentials_.clear();
    combo_credential_->clear();
    label_message_->clear();
    label_message_->hide();

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
    signIn();
}

//--------------------------------------------------------------------------------------------------
void CredentialPanel::onCurrentCredentialChanged()
{
    sign_in_button_->setEnabled(combo_credential_->currentData().isValid());
}

//--------------------------------------------------------------------------------------------------
void CredentialPanel::updatePosition()
{
    const QRect parent_rect = parentWidget()->rect();
    move(parent_rect.center().x() - width() / 2, parent_rect.bottom() - height() - kMargin);
}
