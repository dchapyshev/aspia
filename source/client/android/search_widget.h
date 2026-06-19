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

#ifndef CLIENT_ANDROID_SEARCH_WIDGET_H
#define CLIENT_ANDROID_SEARCH_WIDGET_H

#include <QList>
#include <QString>
#include <QVariant>
#include <QWidget>

class Label;
class SearchHighlightDelegate;
class TreeWidget;

class SearchWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SearchWidget(QWidget* parent = nullptr);
    ~SearchWidget() final;

    struct Result
    {
        QString title;
        QString subtitle;
        QString icon_file_path;
        QVariant data; // Opaque payload handed back to the owner on activation.
    };

    // Shows |results| for |query|; the query also drives the match highlighting. An empty list with a
    // non-empty query shows the "nothing found" hint instead of the blank initial state.
    void setResults(const QList<Result>& results, const QString& query);

    // Clears the results (e.g. when the screen is opened).
    void reset();

    void retranslate();

signals:
    void sig_activated(const QVariant& data);

private:
    TreeWidget* results_;
    SearchHighlightDelegate* delegate_;
    Label* empty_label_;

    Q_DISABLE_COPY_MOVE(SearchWidget)
};

#endif // CLIENT_ANDROID_SEARCH_WIDGET_H
