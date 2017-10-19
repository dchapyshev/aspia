//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__OUTPUT_H
#define _ASPIA_UI__SYSTEM_INFO__OUTPUT_H

#include "protocol/category.h"

#include <memory>
#include <string>

namespace aspia {

class OutputProxy;

class Output
{
public:
    Output();
    virtual ~Output();

    std::shared_ptr<OutputProxy> output_proxy() const { return proxy_; }

    class Document
    {
    public:
        Document(std::shared_ptr<OutputProxy> output, const std::string& name);
        ~Document();

    private:
        std::shared_ptr<OutputProxy> output_;
        DISALLOW_COPY_AND_ASSIGN(Document);
    };

    class Table
    {
    public:
        Table(std::shared_ptr<OutputProxy> output,
              const std::string& name);
        ~Table();

    private:
        std::shared_ptr<OutputProxy> output_;
        DISALLOW_COPY_AND_ASSIGN(Table);
    };

    class TableHeader
    {
    public:
        TableHeader(std::shared_ptr<OutputProxy> output);
        ~TableHeader();

    private:
        std::shared_ptr<OutputProxy> output_;
        DISALLOW_COPY_AND_ASSIGN(TableHeader);
    };

    class Group
    {
    public:
        Group(std::shared_ptr<OutputProxy> output,
              const std::string& name,
              Category::IconId icon_id);
        ~Group();

    private:
        std::shared_ptr<OutputProxy> output_;
        DISALLOW_COPY_AND_ASSIGN(Group);
    };

    class Row
    {
    public:
        Row(std::shared_ptr<OutputProxy> output, Category::IconId icon_id);
        ~Row();

    private:
        std::shared_ptr<OutputProxy> output_;
        DISALLOW_COPY_AND_ASSIGN(Row);
    };

protected:
    virtual void StartDocument(const std::string& name) = 0;
    virtual void EndDocument() = 0;

    virtual void StartTable(const std::string& name) = 0;
    virtual void EndTable() = 0;

    virtual void StartTableHeader() = 0;
    virtual void EndTableHeader() = 0;
    virtual void AddHeaderItem(const std::string& name, int width) = 0;

    virtual void StartGroup(const std::string& name, Category::IconId icon_id) = 0;
    virtual void EndGroup() = 0;
    virtual void AddParam(Category::IconId icon_id,
                          const std::string& param,
                          const std::string& value,
                          const char* unit) = 0;

    virtual void StartRow(Category::IconId icon_id) = 0;
    virtual void EndRow() = 0;
    virtual void AddValue(const std::string& value, const char* unit) = 0;

private:
    friend class OutputProxy;

    std::shared_ptr<OutputProxy> proxy_;
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__OUTPUT_H
