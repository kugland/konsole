/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.countm>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Config
#include "config-konsole.h"

// Own
#include "NullProcessInfo.h"
#include "ProcessInfo.h"
#include "UnixProcessInfo.h"
#include "SSHProcessInfo.h"

// Unix
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>

// Qt
#include <QDir>
#include <QFileInfo>
#include <QHostInfo>
#include <QStringList>
#include <QTextStream>

// KDE
#include <KConfigGroup>
#include <KSharedConfig>
#include <KUser>

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#endif

#if defined(Q_OS_MACOS)
#include <libproc.h>
#include <qplatformdefs.h>
#endif

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <sys/types.h>

#include <sys/syslimits.h>
#include <sys/user.h>
#if defined(Q_OS_FREEBSD)
#include <libutil.h>
#include <sys/param.h>
#include <sys/queue.h>
#endif
#endif

using namespace Konsole;

ProcessInfo::ProcessInfo(int pid)
    : _fields(ARGUMENTS) // arguments
    // are currently always valid,
    // they just return an empty
    // vector / map respectively
    // if no arguments
    // have been explicitly set
    , _pid(pid)
    , _parentPid(0)
    , _foregroundPid(0)
    , _userId(0)
    , _lastError(NoError)
    , _name(QString())
    , _userName(QString())
    , _userHomeDir(QString())
    , _currentDir(QString())
    , _userNameRequired(true)
    , _arguments(QVector<QString>())
{
}

ProcessInfo::Error ProcessInfo::error() const
{
    return _lastError;
}

void ProcessInfo::setError(Error error)
{
    _lastError = error;
}

void ProcessInfo::update()
{
    readCurrentDir(_pid);
}

QString ProcessInfo::validCurrentDir() const
{
    bool ok = false;

    // read current dir, if an error occurs try the parent as the next
    // best option
    int currentPid = parentPid(&ok);
    QString dir = currentDir(&ok);
    while (!ok && currentPid != 0) {
        ProcessInfo *current = ProcessInfo::newInstance(currentPid);
        current->update();
        currentPid = current->parentPid(&ok);
        dir = current->currentDir(&ok);
        delete current;
    }

    return dir;
}

QStringList ProcessInfo::_commonDirNames;

QStringList ProcessInfo::commonDirNames()
{
    static bool forTheFirstTime = true;

    if (forTheFirstTime) {
        const KSharedConfigPtr &config = KSharedConfig::openConfig();
        const KConfigGroup &configGroup = config->group("ProcessInfo");
        // Need to make a local copy so the begin() and end() point to the same QList
        _commonDirNames = configGroup.readEntry("CommonDirNames", QStringList());
        _commonDirNames.removeDuplicates();

        forTheFirstTime = false;
    }

    return _commonDirNames;
}

QString ProcessInfo::formatShortDir(const QString &input) const
{
    if (input == QLatin1Char('/')) {
        return QStringLiteral("/");
    }

    QString result;

    const QStringList parts = input.split(QDir::separator());

    QStringList dirNamesToShorten = commonDirNames();

    // go backwards through the list of the path's parts
    // adding abbreviations of common directory names
    // and stopping when we reach a dir name which is not
    // in the commonDirNames set
    for (auto it = parts.crbegin(), endIt = parts.crend(); it != endIt; ++it) {
        const QString &part = *it;
        if (dirNamesToShorten.contains(part)) {
            result.prepend(QDir::separator() + part[0]);
        } else {
            result.prepend(part);
            break;
        }
    }

    return result;
}

QVector<QString> ProcessInfo::arguments(bool *ok) const
{
    *ok = _fields.testFlag(ARGUMENTS);

    return _arguments;
}

bool ProcessInfo::isValid() const
{
    return _fields.testFlag(PROCESS_ID);
}

int ProcessInfo::pid(bool *ok) const
{
    *ok = _fields.testFlag(PROCESS_ID);

    return _pid;
}

int ProcessInfo::parentPid(bool *ok) const
{
    *ok = _fields.testFlag(PARENT_PID);

    return _parentPid;
}

int ProcessInfo::foregroundPid(bool *ok) const
{
    *ok = _fields.testFlag(FOREGROUND_PID);

    return _foregroundPid;
}

QString ProcessInfo::name(bool *ok) const
{
    *ok = _fields.testFlag(NAME);

    return _name;
}

int ProcessInfo::userId(bool *ok) const
{
    *ok = _fields.testFlag(UID);

    return _userId;
}

QString ProcessInfo::userName() const
{
    return _userName;
}

QString ProcessInfo::userHomeDir() const
{
    return _userHomeDir;
}

QString ProcessInfo::localHost()
{
    return QHostInfo::localHostName();
}

void ProcessInfo::setPid(int pid)
{
    _pid = pid;
    _fields |= PROCESS_ID;
}

void ProcessInfo::setUserId(int uid)
{
    _userId = uid;
    _fields |= UID;
}

void ProcessInfo::setUserName(const QString &name)
{
    _userName = name;
    setUserHomeDir();
}

void ProcessInfo::setUserHomeDir()
{
    const QString &usersName = userName();
    if (!usersName.isEmpty()) {
        _userHomeDir = KUser(usersName).homeDir();
    } else {
        _userHomeDir = QDir::homePath();
    }
}

void ProcessInfo::setParentPid(int pid)
{
    _parentPid = pid;
    _fields |= PARENT_PID;
}

void ProcessInfo::setForegroundPid(int pid)
{
    _foregroundPid = pid;
    _fields |= FOREGROUND_PID;
}

void ProcessInfo::setUserNameRequired(bool need)
{
    _userNameRequired = need;
}

bool ProcessInfo::userNameRequired() const
{
    return _userNameRequired;
}

QString ProcessInfo::currentDir(bool *ok) const
{
    if (ok != nullptr) {
        *ok = (_fields & CURRENT_DIR) != 0;
    }

    return _currentDir;
}

void ProcessInfo::setCurrentDir(const QString &dir)
{
    _fields |= CURRENT_DIR;
    _currentDir = dir;
}

void ProcessInfo::setName(const QString &name)
{
    _name = name;
    _fields |= NAME;
}

void ProcessInfo::addArgument(const QString &argument)
{
    _arguments << argument;
}

void ProcessInfo::clearArguments()
{
    _arguments.clear();
}

void ProcessInfo::setFileError(QFile::FileError error)
{
    switch (error) {
    case QFile::PermissionsError:
        setError(ProcessInfo::PermissionsError);
        break;
    case QFile::NoError:
        setError(ProcessInfo::NoError);
        break;
    default:
        setError(ProcessInfo::UnknownError);
    }
}

#if defined(Q_OS_LINUX)
class LinuxProcessInfo : public UnixProcessInfo
{
public:
    explicit LinuxProcessInfo(int pid)
        : UnixProcessInfo(pid)
    {
    }

protected:
    bool readCurrentDir(int pid) override
    {
        char path_buffer[MAXPATHLEN + 1];
        path_buffer[MAXPATHLEN] = 0;
        QByteArray procCwd = QFile::encodeName(QStringLiteral("/proc/%1/cwd").arg(pid));
        const auto length = static_cast<int>(readlink(procCwd.constData(), path_buffer, MAXPATHLEN));
        if (length == -1) {
            setError(UnknownError);
            return false;
        }

        path_buffer[length] = '\0';
        QString path = QFile::decodeName(path_buffer);

        setCurrentDir(path);
        return true;
    }

private:
    bool readProcInfo(int pid) override
    {
        // indices of various fields within the process status file which
        // contain various information about the process
        const int PARENT_PID_FIELD = 3;
        const int PROCESS_NAME_FIELD = 1;
        const int GROUP_PROCESS_FIELD = 7;

        QString parentPidString;
        QString processNameString;
        QString foregroundPidString;
        QString uidLine;
        QString uidString;
        QStringList uidStrings;

        // For user id read process status file ( /proc/<pid>/status )
        //  Can not use getuid() due to it does not work for 'su'
        QFile statusInfo(QStringLiteral("/proc/%1/status").arg(pid));
        if (statusInfo.open(QIODevice::ReadOnly)) {
            QTextStream stream(&statusInfo);
            QString statusLine;
            do {
                statusLine = stream.readLine();
                if (statusLine.startsWith(QLatin1String("Uid:"))) {
                    uidLine = statusLine;
                }
            } while (!statusLine.isNull() && uidLine.isNull());

            uidStrings << uidLine.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
            // Must be 5 entries: 'Uid: %d %d %d %d' and
            // uid string must be less than 5 chars (uint)
            if (uidStrings.size() == 5) {
                uidString = uidStrings[1];
            }
            if (uidString.size() > 5) {
                uidString.clear();
            }

            bool ok = false;
            const int uid = uidString.toInt(&ok);
            if (ok) {
                setUserId(uid);
            }
            // This will cause constant opening of /etc/passwd
            if (userNameRequired()) {
                readUserName();
                setUserNameRequired(false);
            }
        } else {
            setFileError(statusInfo.error());
            return false;
        }

        // read process status file ( /proc/<pid/stat )
        //
        // the expected file format is a list of fields separated by spaces, using
        // parentheses to escape fields such as the process name which may itself contain
        // spaces:
        //
        // FIELD FIELD (FIELD WITH SPACES) FIELD FIELD
        //
        QFile processInfo(QStringLiteral("/proc/%1/stat").arg(pid));
        if (processInfo.open(QIODevice::ReadOnly)) {
            QTextStream stream(&processInfo);
            const QString &data = stream.readAll();

            int stack = 0;
            int field = 0;
            int pos = 0;

            while (pos < data.count()) {
                QChar c = data[pos];

                if (c == QLatin1Char('(')) {
                    stack++;
                } else if (c == QLatin1Char(')')) {
                    stack--;
                } else if (stack == 0 && c == QLatin1Char(' ')) {
                    field++;
                } else {
                    switch (field) {
                    case PARENT_PID_FIELD:
                        parentPidString.append(c);
                        break;
                    case PROCESS_NAME_FIELD:
                        processNameString.append(c);
                        break;
                    case GROUP_PROCESS_FIELD:
                        foregroundPidString.append(c);
                        break;
                    }
                }

                pos++;
            }
        } else {
            setFileError(processInfo.error());
            return false;
        }

        // check that data was read successfully
        bool ok = false;
        const int foregroundPid = foregroundPidString.toInt(&ok);
        if (ok) {
            setForegroundPid(foregroundPid);
        }

        const int parentPid = parentPidString.toInt(&ok);
        if (ok) {
            setParentPid(parentPid);
        }

        if (!processNameString.isEmpty()) {
            setName(processNameString);
        }

        // update object state
        setPid(pid);

        return ok;
    }

    bool readArguments(int pid) override
    {
        // read command-line arguments file found at /proc/<pid>/cmdline
        // the expected format is a list of strings delimited by null characters,
        // and ending in a double null character pair.

        QFile argumentsFile(QStringLiteral("/proc/%1/cmdline").arg(pid));
        if (argumentsFile.open(QIODevice::ReadOnly)) {
            QTextStream stream(&argumentsFile);
            const QString &data = stream.readAll();

            const QStringList &argList = data.split(QLatin1Char('\0'));

            for (const QString &entry : argList) {
                if (!entry.isEmpty()) {
                    addArgument(entry);
                }
            }
        } else {
            setFileError(argumentsFile.error());
        }

        return true;
    }
};

#elif defined(Q_OS_FREEBSD)
class FreeBSDProcessInfo : public UnixProcessInfo
{
public:
    explicit FreeBSDProcessInfo(int pid)
        : UnixProcessInfo(pid)
    {
    }

protected:
    bool readCurrentDir(int pid) override
    {
#if defined(HAVE_OS_DRAGONFLYBSD)
        char buf[PATH_MAX];
        int managementInfoBase[4];
        size_t len;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_CWD;
        managementInfoBase[3] = pid;

        len = sizeof(buf);
        if (sysctl(managementInfoBase, 4, buf, &len, NULL, 0) == -1) {
            return false;
        }

        setCurrentDir(QString::fromUtf8(buf));

        return true;
#else
        int numrecords;
        struct kinfo_file *info = nullptr;

        info = kinfo_getfile(pid, &numrecords);

        if (!info) {
            return false;
        }

        for (int i = 0; i < numrecords; ++i) {
            if (info[i].kf_fd == KF_FD_TYPE_CWD) {
                setCurrentDir(QString::fromUtf8(info[i].kf_path));

                free(info);
                return true;
            }
        }

        free(info);
        return false;
#endif
    }

private:
    bool readProcInfo(int pid) override
    {
        int managementInfoBase[4];
        size_t mibLength;
        struct kinfo_proc *kInfoProc;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;

        if (sysctl(managementInfoBase, 4, NULL, &mibLength, NULL, 0) == -1) {
            return false;
        }

        kInfoProc = new struct kinfo_proc[mibLength];

        if (sysctl(managementInfoBase, 4, kInfoProc, &mibLength, NULL, 0) == -1) {
            delete[] kInfoProc;
            return false;
        }

#if defined(HAVE_OS_DRAGONFLYBSD)
        setName(QString::fromUtf8(kInfoProc->kp_comm));
        setPid(kInfoProc->kp_pid);
        setParentPid(kInfoProc->kp_ppid);
        setForegroundPid(kInfoProc->kp_pgid);
        setUserId(kInfoProc->kp_uid);
#else
        setName(QString::fromUtf8(kInfoProc->ki_comm));
        setPid(kInfoProc->ki_pid);
        setParentPid(kInfoProc->ki_ppid);
        setForegroundPid(kInfoProc->ki_pgid);
        setUserId(kInfoProc->ki_uid);
#endif

        readUserName();

        delete[] kInfoProc;
        return true;
    }

    bool readArguments(int pid) override
    {
        char args[ARG_MAX];
        int managementInfoBase[4];
        size_t len;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_ARGS;
        managementInfoBase[3] = pid;

        len = sizeof(args);
        if (sysctl(managementInfoBase, 4, args, &len, NULL, 0) == -1) {
            return false;
        }

        // len holds the length of the string
        const QStringList argurments = QString::fromLocal8Bit(args, len).split(QLatin1Char('\u0000'));
        for (const QString &value : argurments) {
            if (!value.isEmpty()) {
                addArgument(value);
            }
        }

        return true;
    }
};

#elif defined(Q_OS_OPENBSD)
class OpenBSDProcessInfo : public UnixProcessInfo
{
public:
    explicit OpenBSDProcessInfo(int pid)
        : UnixProcessInfo(pid)
    {
    }

protected:
    bool readCurrentDir(int pid) override
    {
        char buf[PATH_MAX];
        int managementInfoBase[3];
        size_t len;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC_CWD;
        managementInfoBase[2] = pid;

        len = sizeof(buf);
        if (::sysctl(managementInfoBase, 3, buf, &len, NULL, 0) == -1) {
            qWarning() << "sysctl() call failed with code" << errno;
            return false;
        }

        setCurrentDir(QString::fromUtf8(buf));
        return true;
    }

private:
    bool readProcInfo(int pid) override
    {
        int managementInfoBase[6];
        size_t mibLength;
        struct kinfo_proc *kInfoProc;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;
        managementInfoBase[4] = sizeof(struct kinfo_proc);
        managementInfoBase[5] = 1;

        if (::sysctl(managementInfoBase, 6, NULL, &mibLength, NULL, 0) == -1) {
            qWarning() << "first sysctl() call failed with code" << errno;
            return false;
        }

        kInfoProc = (struct kinfo_proc *)malloc(mibLength);

        if (::sysctl(managementInfoBase, 6, kInfoProc, &mibLength, NULL, 0) == -1) {
            qWarning() << "second sysctl() call failed with code" << errno;
            free(kInfoProc);
            return false;
        }

        setName(QString::fromUtf8(kInfoProc->p_comm));
        setPid(kInfoProc->p_pid);
        setParentPid(kInfoProc->p_ppid);
        setForegroundPid(kInfoProc->p_tpgid);
        setUserId(kInfoProc->p_uid);
        setUserName(QString::fromUtf8(kInfoProc->p_login));

        free(kInfoProc);
        return true;
    }

    char **readProcArgs(int pid, int whatMib)
    {
        void *buf = NULL;
        void *nbuf;
        size_t len = 4096;
        int rc = -1;
        int managementInfoBase[4];

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC_ARGS;
        managementInfoBase[2] = pid;
        managementInfoBase[3] = whatMib;

        do {
            len *= 2;
            nbuf = realloc(buf, len);
            if (nbuf == NULL) {
                break;
            }

            buf = nbuf;
            rc = ::sysctl(managementInfoBase, 4, buf, &len, NULL, 0);
            qWarning() << "sysctl() call failed with code" << errno;
        } while (rc == -1 && errno == ENOMEM);

        if (nbuf == NULL || rc == -1) {
            free(buf);
            return NULL;
        }

        return (char **)buf;
    }

    bool readArguments(int pid) override
    {
        char **argv;

        argv = readProcArgs(pid, KERN_PROC_ARGV);
        if (argv == NULL) {
            return false;
        }

        for (char **p = argv; *p != NULL; p++) {
            addArgument(QString::fromUtf8(*p));
        }
        free(argv);
        return true;
    }
};

#elif defined(Q_OS_MACOS)
class MacProcessInfo : public UnixProcessInfo
{
public:
    explicit MacProcessInfo(int pid)
        : UnixProcessInfo(pid)
    {
    }

protected:
    bool readCurrentDir(int pid) override
    {
        struct proc_vnodepathinfo vpi;
        const int nb = proc_pidinfo(pid, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi));
        if (nb == sizeof(vpi)) {
            setCurrentDir(QString::fromUtf8(vpi.pvi_cdir.vip_path));
            return true;
        }
        return false;
    }

private:
    bool readProcInfo(int pid) override
    {
        int managementInfoBase[4];
        size_t mibLength;
        struct kinfo_proc *kInfoProc;
        QT_STATBUF statInfo;

        // Find the tty device of 'pid' (Example: /dev/ttys001)
        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;

        if (sysctl(managementInfoBase, 4, nullptr, &mibLength, nullptr, 0) == -1) {
            return false;
        } else {
            kInfoProc = new struct kinfo_proc[mibLength];
            if (sysctl(managementInfoBase, 4, kInfoProc, &mibLength, nullptr, 0) == -1) {
                delete[] kInfoProc;
                return false;
            } else {
                const QString deviceNumber = QString::fromUtf8(devname(((&kInfoProc->kp_eproc)->e_tdev), S_IFCHR));
                const QString fullDeviceName = QStringLiteral("/dev/") + deviceNumber.rightJustified(3, QLatin1Char('0'));

                setParentPid(kInfoProc->kp_eproc.e_ppid);
                setForegroundPid(kInfoProc->kp_eproc.e_pgid);

                delete[] kInfoProc;

                const QByteArray deviceName = fullDeviceName.toLatin1();
                const char *ttyName = deviceName.data();

                if (QT_STAT(ttyName, &statInfo) != 0) {
                    return false;
                }

                // Find all processes attached to ttyName
                managementInfoBase[0] = CTL_KERN;
                managementInfoBase[1] = KERN_PROC;
                managementInfoBase[2] = KERN_PROC_TTY;
                managementInfoBase[3] = statInfo.st_rdev;

                mibLength = 0;
                if (sysctl(managementInfoBase, sizeof(managementInfoBase) / sizeof(int), nullptr, &mibLength, nullptr, 0) == -1) {
                    return false;
                }

                kInfoProc = new struct kinfo_proc[mibLength];
                if (sysctl(managementInfoBase, sizeof(managementInfoBase) / sizeof(int), kInfoProc, &mibLength, nullptr, 0) == -1) {
                    return false;
                }

                // The foreground program is the first one
                setName(QString::fromUtf8(kInfoProc->kp_proc.p_comm));

                delete[] kInfoProc;
            }
            setPid(pid);
        }
        return true;
    }

    bool readArguments(int pid) override
    {
        Q_UNUSED(pid)
        return false;
    }
};

#elif defined(Q_OS_SOLARIS)
// The procfs structure definition requires off_t to be
// 32 bits, which only applies if FILE_OFFSET_BITS=32.
// Futz around here to get it to compile regardless,
// although some of the structure sizes might be wrong.
// Fortunately, the structures we actually use don't use
// off_t, and we're safe.
#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
#undef _FILE_OFFSET_BITS
#endif
#include <procfs.h>

class SolarisProcessInfo : public UnixProcessInfo
{
public:
    explicit SolarisProcessInfo(int pid)
        : UnixProcessInfo(pid)
    {
    }

protected:
    // FIXME: This will have the same issues as BKO 251351; the Linux
    // version uses readlink.
    bool readCurrentDir(int pid) override
    {
        QFileInfo info(QString("/proc/%1/path/cwd").arg(pid));
        const bool readable = info.isReadable();

        if (readable && info.isSymLink()) {
            setCurrentDir(info.symLinkTarget());
            return true;
        } else {
            if (!readable) {
                setError(PermissionsError);
            } else {
                setError(UnknownError);
            }

            return false;
        }
    }

private:
    bool readProcInfo(int pid) override
    {
        QFile psinfo(QString("/proc/%1/psinfo").arg(pid));
        if (psinfo.open(QIODevice::ReadOnly)) {
            struct psinfo info;
            if (psinfo.read((char *)&info, sizeof(info)) != sizeof(info)) {
                return false;
            }

            setParentPid(info.pr_ppid);
            setForegroundPid(info.pr_pgid);
            setName(info.pr_fname);
            setPid(pid);

            // Bogus, because we're treating the arguments as one single string
            info.pr_psargs[PRARGSZ - 1] = 0;
            addArgument(info.pr_psargs);
        }
        return true;
    }

    bool readArguments(int /*pid*/) override
    {
        // Handled in readProcInfo()
        return false;
    }
};
#endif

ProcessInfo *ProcessInfo::newInstance(int pid)
{
    ProcessInfo *info;
#if defined(Q_OS_LINUX)
    info = new LinuxProcessInfo(pid);
#elif defined(Q_OS_SOLARIS)
    info = new SolarisProcessInfo(pid);
#elif defined(Q_OS_MACOS)
    info = new MacProcessInfo(pid);
#elif defined(Q_OS_FREEBSD)
    info = new FreeBSDProcessInfo(pid);
#elif defined(Q_OS_OPENBSD)
    info = new OpenBSDProcessInfo(pid);
#else
    info = new NullProcessInfo(pid);
#endif
    info->readProcessInfo(pid);
    return info;
}
