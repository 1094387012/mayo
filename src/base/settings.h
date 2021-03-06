/****************************************************************************
** Copyright (c) 2021, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#pragma once

#include "property.h"
#include "property_value_conversion.h"
#include "settings_index.h"

#include <QtCore/QLocale>
#include <QtCore/QObject>
#include <functional>
class QSettings;

namespace Mayo {

class Settings : public QObject, public PropertyGroup {
    Q_OBJECT
public:
    using GroupIndex = Settings_GroupIndex;
    using SectionIndex = Settings_SectionIndex;
    using SettingIndex = Settings_SettingIndex;
    using ExcludePropertyPredicate = std::function<bool(const Property&)>;
    using ResetFunction = std::function<void()>;

    Settings(QObject* parent = nullptr);
    ~Settings();

    void load();
    void loadProperty(SettingIndex index);
    QVariant findValueFromKey(const QString& strKey) const;
    void save();

    void loadPropertyFrom(const QSettings& source, SettingIndex index);
    void loadFrom(const QSettings& source, const ExcludePropertyPredicate& fnExclude = nullptr);
    void saveAs(QSettings* target, const ExcludePropertyPredicate& fnExclude = nullptr);

    const PropertyValueConversion& propertyValueConversion() const;
    void setPropertyValueConversion(const PropertyValueConversion& conv);

    int groupCount() const;
    QByteArray groupIdentifier(GroupIndex index) const;
    QString groupTitle(GroupIndex index) const;
    GroupIndex addGroup(TextId identifier);
    GroupIndex addGroup(QByteArray identifier);
    void setGroupTitle(GroupIndex index, const QString& title);

    void addResetFunction(GroupIndex index, ResetFunction fn);
    void addResetFunction(SectionIndex index, ResetFunction fn);

    int sectionCount(GroupIndex index) const;
    QByteArray sectionIdentifier(SectionIndex index) const;
    QString sectionTitle(SectionIndex index) const;
    bool isDefaultGroupSection(SectionIndex index) const;
    SectionIndex addSection(GroupIndex index, TextId identifier);
    SectionIndex addSection(GroupIndex index, QByteArray identifier);
    void setSectionTitle(SectionIndex index, const QString& title);

    int settingCount(SectionIndex index) const;
    Property* property(SettingIndex index) const;
    SettingIndex findProperty(const Property* property) const;
    SettingIndex addSetting(Property* property, GroupIndex index);
    SettingIndex addSetting(Property* property, SectionIndex index);

    void resetAll();
    void resetGroup(GroupIndex index);
    void resetSection(SectionIndex index);

    // Helpers

    static QByteArray defautLocaleLanguageCode();
    const QLocale& locale() const;
    void setLocale(const QLocale& locale);

signals:
    void changed(Mayo::Property* setting);
    void enabled(Mayo::Property* setting, bool on);

protected:
    void onPropertyChanged(Property* prop) override;
    void onPropertyEnabled(Property* prop, bool on) override;

private:
    class Private;
    Private* const d = nullptr;
};

} // namespace Mayo
