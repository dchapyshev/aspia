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

#include "common/desktop/plain_text_edit.h"

#include <QKeyEvent>
#include <QMimeData>

namespace {

//--------------------------------------------------------------------------------------------------
qsizetype insertedLength(const QKeyEvent* event)
{
    if (event->matches(QKeySequence::InsertParagraphSeparator) ||
        event->matches(QKeySequence::InsertLineSeparator))
    {
        return 1;
    }

    const Qt::KeyboardModifiers modifiers = event->modifiers();
    if (modifiers == Qt::ControlModifier || modifiers == (Qt::ControlModifier | Qt::ShiftModifier))
        return 0;

    const QString text = event->text();
    if (text.isEmpty())
        return 0;

    const QChar first = text.at(0);
    if (!first.isPrint() && !first.isHighSurrogate() && first != QLatin1Char('\t'))
        return 0;

    return text.size();
}

//--------------------------------------------------------------------------------------------------
QString limitedText(const QString& text, int length)
{
    if (length <= 0)
        return QString();

    if (text.at(length - 1).isHighSurrogate())
        --length;

    return text.left(length);
}

} // namespace

//--------------------------------------------------------------------------------------------------
PlainTextEdit::PlainTextEdit(QWidget* parent)
    : QPlainTextEdit(parent)
{
    connect(this, &QPlainTextEdit::textChanged, this, &PlainTextEdit::onTextChanged);
}

//--------------------------------------------------------------------------------------------------
PlainTextEdit::~PlainTextEdit() = default;

//--------------------------------------------------------------------------------------------------
void PlainTextEdit::setMaxLength(int length)
{
    max_length_ = length;
}

//--------------------------------------------------------------------------------------------------
void PlainTextEdit::keyPressEvent(QKeyEvent* event)
{
    if (max_length_ >= 0 && !isReadOnly() && insertedLength(event) > freeLength())
    {
        event->accept();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void PlainTextEdit::insertFromMimeData(const QMimeData* source)
{
    if (max_length_ < 0 || !source)
    {
        QPlainTextEdit::insertFromMimeData(source);
        return;
    }

    const QString text = source->text();
    const int free_length = freeLength();

    if (text.size() <= free_length)
    {
        QPlainTextEdit::insertFromMimeData(source);
        return;
    }

    if (free_length <= 0)
        return;

    QMimeData limited;
    limited.setText(limitedText(text, free_length));
    QPlainTextEdit::insertFromMimeData(&limited);
}

//--------------------------------------------------------------------------------------------------
void PlainTextEdit::inputMethodEvent(QInputMethodEvent* event)
{
    const QString text = event->commitString();
    const int free_length = freeLength() + event->replacementLength();

    if (max_length_ < 0 || text.size() <= free_length)
    {
        QPlainTextEdit::inputMethodEvent(event);
        return;
    }

    QInputMethodEvent limited(event->preeditString(), event->attributes());
    limited.setCommitString(limitedText(text, free_length), event->replacementStart(),
                            event->replacementLength());
    QPlainTextEdit::inputMethodEvent(&limited);
    event->setAccepted(limited.isAccepted());
}

//--------------------------------------------------------------------------------------------------
void PlainTextEdit::onTextChanged()
{
    if (max_length_ < 0)
        return;

    const QTextDocument* doc = document();
    const int length = doc->characterCount() - 1;
    const int excess = length - max_length_;
    if (excess <= 0)
        return;

    QTextCursor cursor = textCursor();

    int end = cursor.position();
    if (end < excess)
        end = length;

    int begin = end - excess;
    if (begin > 0 && doc->characterAt(begin - 1).isHighSurrogate())
        --begin;

    cursor.joinPreviousEditBlock();
    cursor.setPosition(begin);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();
}

//--------------------------------------------------------------------------------------------------
int PlainTextEdit::freeLength() const
{
    const QTextCursor cursor = textCursor();
    const int selected = cursor.selectionEnd() - cursor.selectionStart();

    return max_length_ - (document()->characterCount() - 1) + selected;
}
