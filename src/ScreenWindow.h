/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SCREENWINDOW_H
#define SCREENWINDOW_H

// Qt
#include <QObject>
#include <QPoint>
#include <QRect>

// Konsole
#include "../characters/Character.h"
#include "Screen.h"

namespace Konsole
{
/**
 * Provides a window onto a section of a terminal screen.  A terminal widget can then render
 * the contents of the window and use the window to change the terminal screen's selection
 * in response to mouse or keyboard input.
 *
 * A new ScreenWindow for a terminal session can be created by calling Emulation::createWindow()
 *
 * Use the scrollTo() method to scroll the window up and down on the screen.
 * Use the getImage() method to retrieve the character image which is currently visible in the window.
 *
 * setTrackOutput() controls whether the window moves to the bottom of the associated screen when new
 * lines are added to it.
 *
 * Whenever the output from the underlying screen is changed, the notifyOutputChanged() slot should
 * be called.  This in turn will update the window's position and emit the outputChanged() signal
 * if necessary.
 */
class ScreenWindow : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs a new screen window with the given parent.
     * A screen must be specified by calling setScreen() before calling getImage() or getLineProperties().
     *
     * You should not call this constructor directly, instead use the Emulation::createWindow() method
     * to create a window on the emulation which you wish to view.  This allows the emulation
     * to notify the window when the associated screen has changed and synchronize selection updates
     * between all views on a session.
     */
    explicit ScreenWindow(Screen *screen, QObject *parent = nullptr);
    ~ScreenWindow() override;

    /** Sets the screen which this window looks onto */
    void setScreen(Screen *screen);
    /** Returns the screen which this window looks onto */
    Screen *screen() const;

    /**
     * Returns the image of characters which are currently visible through this window
     * onto the screen.
     *
     * The returned buffer is managed by the ScreenWindow instance and does not need to be
     * deleted by the caller.
     */
    Character *getImage();

    /**
     * Returns the line attributes associated with the lines of characters which
     * are currently visible through this window
     */
    QVector<LineProperty> getLineProperties();

    /**
     * Returns the number of lines which the region of the window
     * specified by scrollRegion() has been scrolled by since the last call
     * to resetScrollCount().  scrollRegion() is in most cases the
     * whole window, but will be a smaller area in, for example, applications
     * which provide split-screen facilities.
     *
     * This is not guaranteed to be accurate, but allows views to optimize
     * rendering by reducing the amount of costly text rendering that
     * needs to be done when the output is scrolled.
     */
    int scrollCount() const;

    /**
     * Resets the count of scrolled lines returned by scrollCount()
     */
    void resetScrollCount();

    /**
     * Returns the area of the window which was last scrolled, this is
     * usually the whole window area.
     *
     * Like scrollCount(), this is not guaranteed to be accurate,
     * but allows views to optimize rendering.
     */
    QRect scrollRegion() const;

    /**
     * What line the next search will start from
     */
    void setCurrentResultLine(int line);
    int currentResultLine() const;

    /**
     * Sets the start of the selection to the given @p line and @p column within
     * the window.
     */
    void setSelectionStart(int column, int line, bool columnMode);
    /**
     * Sets the end of the selection to the given @p line and @p column within
     * the window.
     */
    void setSelectionEnd(int column, int line, bool trimTrailingWhitespace);

    /**
     * Sets the selection as the range specified by line @p start and line @p
     * end in the whole history.
     *
     * Both @p start and @p end are absolute line number in the whole history,
     * not relative line number in the window. This make it possible to select
     * range larger than the window . A good use case is selecting the whole
     * history.
     */
    void setSelectionByLineRange(int start, int end);

    /**
     * Retrieves the start of the selection within the window.
     */
    void getSelectionStart(int &column, int &line);
    /**
     * Retrieves the end of the selection within the window.
     */
    void getSelectionEnd(int &column, int &line);
    /**
     * Returns true if the character at @p line , @p column is part of the selection.
     */
    bool isSelected(int column, int line);
    /**
     * Clears the current selection
     */
    void clearSelection();

    /** Sets the number of lines in the window */
    void setWindowLines(int lines);
    /** Returns the number of lines in the window */
    int windowLines() const;
    /** Returns the number of columns in the window */
    int windowColumns() const;

    /** Returns the total number of lines in the screen */
    int lineCount() const;
    /** Returns the total number of columns in the screen */
    int columnCount() const;

    /** Returns the index of the line which is currently at the top of this window */
    int currentLine() const;

    /**
     * Returns the position of the cursor
     * within the window.
     */
    QPoint cursorPosition() const;

    /**
     * Convenience method. Returns true if the window is currently at the bottom
     * of the screen.
     */
    bool atEndOfOutput() const;

    /** Scrolls the window so that @p line is at the top of the window */
    void scrollTo(int line);

    /** Describes the units which scrollBy() moves the window by. */
    enum RelativeScrollMode {
        /** Scroll the window down by a given number of lines. */
        ScrollLines,
        /**
         * Scroll the window down by a given number of pages, where
         * one page is windowLines() lines
         */
        ScrollPages,
    };

    /**
     * Scrolls the window relative to its current position on the screen.
     *
     * @param mode Specifies whether @p amount refers to the number of lines or the number
     * of pages to scroll.
     * @param amount The number of lines or pages ( depending on @p mode ) to scroll by.  If
     * this number is positive, the view is scrolled down.  If this number is negative, the view
     * is scrolled up.
     * @param fullPage Specifies whether to scroll by full page or half page.
     */
    void scrollBy(RelativeScrollMode mode, int amount, bool fullPage);

    /**
     * Specifies whether the window should automatically move to the bottom
     * of the screen when new output is added.
     *
     * If this is set to true, the window will be moved to the bottom of the associated screen ( see
     * screen() ) when the notifyOutputChanged() method is called.
     */
    void setTrackOutput(bool trackOutput);
    /**
     * Returns whether the window automatically moves to the bottom of the screen as
     * new output is added.  See setTrackOutput()
     */
    bool trackOutput() const;

    /**
     * Returns the text which is currently selected.
     *
     * @param options See Screen::DecodingOptions
     */
    QString selectedText(const Screen::DecodingOptions options) const;

    void updateCurrentLine();

public Q_SLOTS:
    /**
     * Notifies the window that the contents of the associated terminal screen have changed.
     * This moves the window to the bottom of the screen if trackOutput() is true and causes
     * the outputChanged() signal to be emitted.
     */
    void notifyOutputChanged();

Q_SIGNALS:
    /**
     * Emitted when the contents of the associated terminal screen (see screen()) changes.
     */
    void outputChanged();

    void currentResultLineChanged();

    /**
     * Emitted when the screen window is scrolled to a different position.
     *
     * @param line The line which is now at the top of the window.
     */
    void scrolled(int line);

    /** Emitted when the selection is changed. */
    void selectionChanged();

    /** Emitted when e. g. Screen is changed (to alternate or primary), so
     * initial selection offsets etc. are invalidated.
     */
    void screenAboutToChange();

private:
    Q_DISABLE_COPY(ScreenWindow)

    int endWindowLine() const;
    void fillUnusedArea();

    Screen *_screen; // see setScreen() , screen()
    Character *_windowBuffer;
    int _windowBufferSize;
    bool _bufferNeedsUpdate;

    int _windowLines;
    int _currentLine; // see scrollTo() , currentLine()
    int _currentResultLine;
    bool _trackOutput; // see setTrackOutput() , trackOutput()
    int _scrollCount; // count of lines which the window has been scrolled by since
    // the last call to resetScrollCount()
};
}
#endif // SCREENWINDOW_H
