//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__XML_SAX_WRITER_H
#define BASE__XML_SAX_WRITER_H

#include "base/macros_magic.h"

#include <ostream>
#include <stack>

namespace base {

class XmlSaxWriter
{
public:
    XmlSaxWriter(std::ostream& stream);
    ~XmlSaxWriter();

    void startDocument();
    void endDocument();

    void startElement(std::string_view name);
    void endElement();

    void attribute(std::string_view name, std::string_view value);
    void text(std::string_view str);

private:
    void indent();
    void endTag();

    std::ostream& stream_;
    std::stack<std::string> elements_;
    bool has_tag_end_ = true;
    bool has_text_ = false;

    DISALLOW_COPY_AND_ASSIGN(XmlSaxWriter);
};

} // namespace base

#endif // BASE__XML_SAX_WRITER_H
