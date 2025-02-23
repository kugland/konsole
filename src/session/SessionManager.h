/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

// Qt
#include <QExplicitlySharedDataPointer>
#include <QHash>
#include <QList>

// TODO: Move the Property away from Profile.h
#include "profile/Profile.h"

#include "konsolesession_export.h"

class KConfig;

namespace Konsole
{
class Session;
class Profile;

/**
 * Manages running terminal sessions.
 */
class KONSOLESESSION_EXPORT SessionManager : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs a new session manager and loads information about the available
     * profiles.
     */
    SessionManager();

    /**
     * Destroys the SessionManager. All running sessions should be closed
     * (via closeAllSessions()) before the SessionManager is destroyed.
     */
    ~SessionManager() override;

    /**
     * Returns the session manager instance.
     */
    static SessionManager *instance();

    /** Kill all running sessions. */
    void closeAllSessions();

    /**
     * Creates a new session using the settings specified by the specified
     * profile.
     *
     * The new session has no views associated with it.  A new TerminalDisplay view
     * must be created in order to display the output from the terminal session and
     * send keyboard or mouse input to it.
     *
     * @param profile A profile containing the settings for the new session.  If @p profile
     * is null the default profile (see ProfileManager::defaultProfile()) will be used.
     */
    Session *createSession(QExplicitlySharedDataPointer<Profile> profile = QExplicitlySharedDataPointer<Profile>());

    /** Sets the profile associated with a session. */
    void setSessionProfile(Session *session, QExplicitlySharedDataPointer<Profile> profile);

    /** Returns the profile associated with a session. */
    QExplicitlySharedDataPointer<Profile> sessionProfile(Session *session) const;

    /**
     * Returns a list of active sessions.
     */
    const QList<Session *> sessions() const;

    // System session management
    void saveSessions(KConfig *config);
    void restoreSessions(KConfig *config);
    int getRestoreId(Session *session);
    Session *idToSession(int id);
    bool isClosingAllSessions() const;

Q_SIGNALS:
    /**
     * Emitted when a session's settings are updated to match
     * its current profile.
     */
    void sessionUpdated(Session *session);

protected Q_SLOTS:
    /**
     * Called to inform the manager that a session has finished executing.
     *
     * @param session The Session which has finished executing.
     */
    void sessionTerminated(Session *session);

private Q_SLOTS:
    void sessionProfileCommandReceived(Session *session, const QString &text);

    void profileChanged(const QExplicitlySharedDataPointer<Profile> &profile);

private:
    Q_DISABLE_COPY(SessionManager)

    // applies updates to a profile
    // to all sessions currently using that profile
    // if modifiedPropertiesOnly is true, only properties which
    // are set in the profile @p key are updated
    void applyProfile(const QExplicitlySharedDataPointer<Profile> &profile, bool modifiedPropertiesOnly);

    // applies updates to the profile @p profile to the session @p session
    // if modifiedPropertiesOnly is true, only properties which
    // are set in @p profile are update ( ie. properties for which profile->isPropertySet(<property>)
    // returns true )
    void applyProfile(Session *session, const QExplicitlySharedDataPointer<Profile> &profile, bool modifiedPropertiesOnly);

    QList<Session *> _sessions; // list of running sessions

    QHash<Session *, QExplicitlySharedDataPointer<Profile>> _sessionProfiles;
    QHash<Session *, QExplicitlySharedDataPointer<Profile>> _sessionRuntimeProfiles;
    QHash<Session *, int> _restoreMapping;
    bool _isClosingAllSessions;
};

}

#endif // SESSIONMANAGER_H
