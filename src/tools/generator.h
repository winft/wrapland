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

#include <QMap>
#include <QMutex>
#include <QObject>
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
    explicit Argument(QXmlStreamAttributes const& attributes);
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

    QString name() const
    {
        return m_name;
    }
    Type type() const
    {
        return m_type;
    }
    bool isNullAllowed() const
    {
        return m_allowNull;
    }
    QString interface() const
    {
        return m_inteface;
    }
    QString typeAsQt() const;
    QString typeAsServerWl() const;

private:
    Type parseType(QStringRef const& type);
    QString m_name;
    Type m_type = Type::Unknown;
    bool m_allowNull = false;
    QString m_inteface;
};

class Request
{
public:
    explicit Request();
    explicit Request(QString const& name);
    ~Request();

    void addArgument(Argument const& arg)
    {
        m_arguments << arg;
    }

    QString name() const
    {
        return m_name;
    }

    QVector<Argument> arguments() const
    {
        return m_arguments;
    }

    bool isDestructor() const
    {
        return m_destructor;
    }
    bool isFactory() const;

    void markAsDestructor()
    {
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
    explicit Event(QString const& name);
    ~Event();

    void addArgument(Argument const& arg)
    {
        m_arguments << arg;
    }

    QString name() const
    {
        return m_name;
    }

    QVector<Argument> arguments() const
    {
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
    explicit Interface(QXmlStreamAttributes const& attributes);
    virtual ~Interface();

    void addRequest(Request const& request)
    {
        m_requests << request;
    }
    void addEvent(Event const& event)
    {
        m_events << event;
    }

    QString name() const
    {
        return m_name;
    }
    quint32 version() const
    {
        return m_version;
    }
    QString wraplandClientName() const
    {
        return m_clientName;
    }
    QString wraplandServerName() const
    {
        return m_clientName;
    }

    QVector<Request> requests() const
    {
        return m_requests;
    }

    QVector<Event> events() const
    {
        return m_events;
    }

    void markAsGlobal()
    {
        m_global = true;
    }
    bool isGlobal() const
    {
        return m_global;
    }
    void setFactory(Interface* factory)
    {
        m_factory = factory;
    }
    Interface* factory() const
    {
        return m_factory;
    }

    bool isUnstableInterface() const
    {
        return m_name.startsWith(QLatin1String("zwp"));
    }

private:
    QString m_name;
    QString m_clientName;
    quint32 m_version;
    QVector<Request> m_requests;
    QVector<Event> m_events;
    bool m_global = false;
    Interface* m_factory;
};

class Generator : public QObject
{
    Q_OBJECT
public:
    explicit Generator(QObject* parent = nullptr);
    virtual ~Generator();

    void setXmlFileName(QString const& name)
    {
        m_xmlFileName = name;
    }
    void setBaseFileName(QString const& name)
    {
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
    void generatePrivateClass(Interface const& interface);
    void generateClientPrivateClass(Interface const& interface);
    void generateClientPrivateResourceClass(Interface const& interface);
    void generateClientPrivateGlobalClass(Interface const& interface);
    void generateServerPrivateGlobalFunction(Interface const& interface);
    void generateServerPrivateHeaderGlobalClass(Interface const& interface);
    void generateServerPrivateResourceFunction(Interface const& interface);
    void generateServerPrivateHeaderResourceClass(Interface const& interface);
    void generateServerPrivateInterfaceClass(Interface const& interface);
    void generateServerPrivateGlobalConstructor(Interface const& interface);
    void generateServerPrivateResourceCtorDtorClass(Interface const& interface);
    void generateServerPrivateCallbackDefinitions(Interface const& interface);
    void generateServerPrivateCallbackImpl(Interface const& interface);
    void generateClientCpp(Interface const& interface);
    void generateClass(Interface const& interface);
    void generateClientGlobalClass(Interface const& interface);
    void generateClientResourceClass(Interface const& interface);
    void generateServerGlobalClass(Interface const& interface);
    void generateServerGlobalClassUnstable(Interface const& interface);
    void generateServerResourceClass(Interface const& interface);
    void generateServerResourceClassUnstable(Interface const& interface);
    void generateClientClassQObjectDerived(Interface const& interface);
    void generateClientGlobalClassDoxy(Interface const& interface);
    void generateClientGlobalClassCtor(Interface const& interface);
    void generateClientGlobalClassSetup(Interface const& interface);
    void generateClientResourceClassSetup(Interface const& interface);
    void generateClientClassDtor(Interface const& interface);
    void generateClientClassReleaseDestroy(Interface const& interface);
    void generateClientClassStart(Interface const& interface);
    void generateClientClassCasts(Interface const& interface);
    void generateClientClassSignals(Interface const& interface);
    void generateClientClassDptr(Interface const& interface);
    void generateClientGlobalClassEnd(Interface const& interface);
    void generateClientResourceClassEnd(Interface const& interface);
    void generateClientClassRequests(Interface const& interface);
    void generateClientCppRequests(Interface const& interface);
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
    enum class Project { Client, Server, ServerPrivate };
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
