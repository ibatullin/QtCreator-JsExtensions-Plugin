DEFINES += JEP_LIBRARY

QT += qml quick widgets

CONFIG += c++11

# JsExtensions files

SOURCES += \
    jsextensionsplugin.cpp \
    jepapi.cpp

HEADERS += \
    jsextensionsplugin.h \
    jsextensionsconstants.h \
    jsextensions_global.h \
    jepapi.h

# Qt Creator linking

## set the QTC_SOURCE environment variable to override the setting here
QTCREATOR_SOURCES = $$QTC_SOURCE
isEmpty(QTCREATOR_SOURCES):QTCREATOR_SOURCES=/home/alex/Dev/QtCreator/qt-creator

## set the QTC_BUILD environment variable to override the setting here
IDE_BUILD_TREE = $$QTC_BUILD
isEmpty(IDE_BUILD_TREE):IDE_BUILD_TREE=/home/alex/Dev/QtCreator/qt-creator-build

## uncomment to build plugin into user config directory
## <localappdata>/plugins/<ideversion>
##    where <localappdata> is e.g.
##    "%LOCALAPPDATA%\QtProject\qtcreator" on Windows Vista and later
##    "$XDG_DATA_HOME/data/QtProject/qtcreator" or "~/.local/share/data/QtProject/qtcreator" on Linux
##    "~/Library/Application Support/QtProject/Qt Creator" on Mac
# USE_USER_DESTDIR = yes

PROVIDER = Zhondin

###### If the plugin can be depended upon by other plugins, this code needs to be outsourced to
###### <dirname>_dependencies.pri, where <dirname> is the name of the directory containing the
###### plugin's sources.

QTC_PLUGIN_NAME = JsExtensions
QTC_LIB_DEPENDS += \
    # nothing here at this time

QTC_PLUGIN_DEPENDS += \
    coreplugin

QTC_PLUGIN_RECOMMENDS += \
    # optional plugin dependencies. nothing here at this time

###### End _dependencies.pri contents ######
include($$QTCREATOR_SOURCES/src/qtcreatorplugin.pri)

# make life easier
DEFINES -= QT_NO_CAST_FROM_ASCII

# copy jep plugins
copy_plugins.commands = $(COPY_DIR) \"$$PWD/jep_plugins\" \"$$DESTDIR\"
first.depends = $(first) copy_plugins
export(first.depends)
export(copy_plugins.commands)
QMAKE_EXTRA_TARGETS += first copy_plugins

