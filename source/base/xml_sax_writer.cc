//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/xml_sax_writer.h"

#include "base/logging.h"

namespace base {

namespace {

bool hasBadChar(std::string_view str)
{
    for (const auto ch : str)
    {
        switch (ch)
        {
            case '<':
            case '>':
            case '&':
            case '"':
            case '\'':
                return true;

            default:
                break;
        }
    }

    return false;
}

std::string escapeString(std::string_view str)
{
    if (str.empty())
        return std::string();

    std::string result;

    for (const auto ch : str)
    {
        switch (ch)
        {
            case '<':
                result += "&lt;";
                break;

            case '>':
                result += "&gt;";
                break;

            case '&':
                result += "&amp;";
                break;

            case '"':
                result += "&quot;";
                break;

            case '\'':
                result += "&apos;";
                break;

            default:
                result += ch;
                break;
        }
    }

    return result;
}

} // namespace

XmlSaxWriter::XmlSaxWriter(std::ostream& stream)
    : stream_(stream)
{
    // Nothing
}

XmlSaxWriter::~XmlSaxWriter() = default;

void XmlSaxWriter::startDocument()
{
    stream_ << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
}

void XmlSaxWriter::endDocument()
{
    DCHECK(elements_.empty());
    DCHECK(has_tag_end_);
}

void XmlSaxWriter::startElement(std::string_view name)
{
    DCHECK(!name.empty());
    DCHECK(!hasBadChar(name));

    elements_.emplace(name);

    if (!has_tag_end_)
        endTag();

    stream_ << std::endl;

    indent();

    stream_ << '<' << name;
    has_tag_end_ = false;
}

void XmlSaxWriter::endElement()
{
    DCHECK(!elements_.empty());

    if (!has_tag_end_)
        endTag();

    if (!has_text_)
    {
        stream_ << std::endl;
        indent();
    }

    stream_ << "</" << elements_.top() << '>';

    elements_.pop();
    has_text_ = false;
}

void XmlSaxWriter::attribute(std::string_view name, std::string_view value)
{
    DCHECK(!name.empty());
    DCHECK(!hasBadChar(name));

    stream_ << ' ' << name << "=\"" << escapeString(value) << '"';
}

void XmlSaxWriter::text(std::string_view str)
{
    if (!has_tag_end_)
        endTag();

    stream_ << escapeString(str);
    has_text_ = true;
}

void XmlSaxWriter::indent()
{
    if (elements_.empty())
        return;

    for (size_t i = 0; i < elements_.size() - 1; ++i)
        stream_ << "\t";
}

void XmlSaxWriter::endTag()
{
    stream_ << '>';
    has_tag_end_ = true;
}

} // namespace base
