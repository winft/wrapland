/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#pragma once

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QThreadStorage>
#include <QWaitCondition>
#include <QXmlStreamReader>

class QTextStream;

namespace Wrapland::Tools
{

class Argument
{
public:
    explicit Argument();
    explicit Argument(const QXmlStreamAttributes &attributes);
    ~Argument();

    enum class Type {
        Unknown,
        NewId,
        Destructor,
        Object,
        FileDescriptor,
        Fixed,
        Uint,
        Int,
        String
    };

    QString name() const {
        return m_name;
    }
    Type type() const {
        return m_type;
    }
    bool isNullAllowed() const {
        return m_allowNull;
    }
    QString interface() const {
        return m_inteface;
    }
    QString typeAsQt() const;
    QString typeAsServerWl() const;

private:
    Type parseType(const QStringRef &type);
    QString m_name;
    Type m_type = Type::Unknown;
    bool m_allowNull = false;
    QString m_inteface;
};

class Request
{
public:
    explicit Request();
    explicit Request(const QString &name);
    ~Request();

    void addArgument(const Argument &arg) {
        m_arguments << arg;
    }

    QString name() const {
        return m_name;
    }

    QVector<Argument> arguments() const {
        return m_arguments;
    }

    bool isDestructor() const {
        return m_destructor;
    }
    bool isFactory() const;

    void markAsDestructor() {
        m_destructor = true;
    }

private:
    QString m_name;
    QVector<Argument> m_arguments;
    bool m_destructor = false;
};

class Event
{
public:
    explicit Event();
    explicit Event(const QString &name);
    ~Event();

    void addArgument(const Argument &arg) {
        m_arguments << arg;
    }

    QString name() const {
        return m_name;
    }

    QVector<Argument> arguments() const {
        return m_arguments;
    }

private:
    QString m_name;
    QVector<Argument> m_arguments;
};

class Interface
{
public:
    explicit Interface();
    explicit Interface(const QXmlStreamAttributes &attributes);
    virtual ~Interface();

    void addRequest(const Request &request) {
        m_requests << request;
    }
    void addEvent(const Event &event) {
        m_events << event;
    }

    QString name() const {
        return m_name;
    }
    quint32 version() const {
        return m_version;
    }
    QString wraplandClientName() const {
        return m_clientName;
    }
    QString wraplandServerName() const {
        return m_clientName;
    }

    QVector<Request> requests() const {
        return m_requests;
    }

    QVector<Event> events() const {
        return m_events;
    }

    void markAsGlobal() {
        m_global = true;
    }
    bool isGlobal() const {
        return m_global;
    }
    void setFactory(Interface *factory) {
        m_factory = factory;
    }
    Interface *factory() const {
        return m_factory;
    }

    bool isUnstableInterface() const {
        return m_name.startsWith(QLatin1String("zwp"));
    }

private:
    QString m_name;
    QString m_clientName;
    quint32 m_version;
    QVector<Request> m_requests;
    QVector<Event> m_events;
    bool m_global = false;
    Interface *m_factory;
};


class Generator : public QObject
{
    Q_OBJECT
public:
    explicit Generator(QObject *parent = nullptr);
    virtual ~Generator();

    void setXmlFileName(const QString &name) {
        m_xmlFileName = name;
    }
    void setBaseFileName(const QString &name) {
        m_baseFileName = name;
    }
    void start();

private:
    void generateCopyrightHeader();
    void generateStartIncludeGuard();
    void generateEndIncludeGuard();
    void generateStartNamespace();
    void generateEndNamespace();
    void generateHeaderIncludes();
    void generateCppIncludes();
    void generatePrivateClass(const Interface &interface);
    void generateClientPrivateClass(const Interface &interface);
    void generateClientPrivateResourceClass(const Interface &interface);
    void generateClientPrivateGlobalClass(const Interface &interface);
    void generateServerPrivateGlobalFunction(const Interface &interface);
    void generateServerPrivateHeaderGlobalClass(const Interface &interface);
    void generateServerPrivateResourceFunction(const Interface &interface);
    void generateServerPrivateHeaderResourceClass(const Interface &interface);
    void generateServerPrivateInterfaceClass(const Interface &interface);
    void generateServerPrivateGlobalConstructor(const Interface &interface);
    void generateServerPrivateResourceCtorDtorClass(const Interface &interface);
    void generateServerPrivateCallbackDefinitions(const Interface &interface);
    void generateServerPrivateCallbackImpl(const Interface &interface);
    void generateClientCpp(const Interface &interface);
    void generateClass(const Interface &interface);
    void generateClientGlobalClass(const Interface &interface);
    void generateClientResourceClass(const Interface &interface);
    void generateServerGlobalClass(const Interface &interface);
    void generateServerGlobalClassUnstable(const Interface &interface);
    void generateServerResourceClass(const Interface &interface);
    void generateServerResourceClassUnstable(const Interface &interface);
    void generateClientClassQObjectDerived(const Interface &interface);
    void generateClientGlobalClassDoxy(const Interface &interface);
    void generateClientGlobalClassCtor(const Interface &interface);
    void generateClientGlobalClassSetup(const Interface &interface);
    void generateClientResourceClassSetup(const Interface &interface);
    void generateClientClassDtor(const Interface &interface);
    void generateClientClassReleaseDestroy(const Interface &interface);
    void generateClientClassStart(const Interface &interface);
    void generateClientClassCasts(const Interface &interface);
    void generateClientClassSignals(const Interface &interface);
    void generateClientClassDptr(const Interface &interface);
    void generateClientGlobalClassEnd(const Interface &interface);
    void generateClientResourceClassEnd(const Interface &interface);
    void generateClientClassRequests(const Interface &interface);
    void generateClientCppRequests(const Interface &interface);
    void generateWaylandForwardDeclarations();
    void generateNamespaceForwardDeclarations();
    void startParseXml();
    void startAuthorNameProcess();
    void startAuthorEmailProcess();
    void startGenerateHeaderFile();
    void startGenerateCppFile();
    void startGenerateServerHeaderFile();
    void startGenerateServerPrivateHeaderFile();
    void startGenerateServerCppFile();

    void checkEnd();

    void parseProtocol();
    Interface parseInterface();
    Request parseRequest();
    Event parseEvent();

    QString projectToName() const;

    QThreadStorage<QTextStream*> m_stream;
    QString m_xmlFileName;
    enum class Project {
        Client,
        Server,
        ServerPrivate
    };
    QThreadStorage<Project> m_project;
    QString m_authorName;
    QString m_authorEmail;
    QString m_baseFileName;

    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    QXmlStreamReader m_xmlReader;
    QVector<Interface> m_interfaces;

    int m_finishedCounter = 0;
};

}
