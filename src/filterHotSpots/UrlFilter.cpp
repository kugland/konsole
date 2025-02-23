/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "UrlFilter.h"

using namespace Konsole;

#include "UrlFilterHotspot.h"

// Note:  Altering these regular expressions can have a major effect on the performance of the filters
// used for finding URLs in the text, especially if they are very general and could match very long
// pieces of text.
// Please be careful when altering them.

// FullUrlRegExp is implemented based on:
// https://datatracker.ietf.org/doc/html/rfc3986
// See above URL for what "unreserved", "pct-encoded" ...etc mean, also
// for the regex used for each part of the url being matched against.
//
// It deviates from rfc3986:
// - We only recognize URIs with authority (even if it is an empty authority)
// - We match URIs starting with 'www.'
// - "userinfo" is assumed to have a single ':' character
// - We _don't_ match IPv6 addresses (e.g. http://[2010:836B:4179::836B:4179])
//   or IPvFuture
// - "port" (':1234'), if present, is assumed to be non-empty
// - We don't check the validity of percent-encoded characters
//   (e.g. "www.example.com/foo%XXbar")

// All () groups are non-capturing (by using "(?:)" notation)
// less bookkeeping on the PCRE engine side

// scheme://
// - Must start with an ASCII letter, preceeded by any non-word character,
//   so "http" but not "mhttp"
static const char scheme_or_www[] = "(?<=^|[\\s\\[\\]()'\"])(?:www\\.|[a-z][a-z0-9+\\-.]*+://)";

// unreserved / pct-encoded / sub-delims
#define COMMON_1 "a-z0-9\\-._~%!$&'()*+,;="

/* clang-format off */
// user:password@
static const char userInfo[] =
    "(?:"
    "[" COMMON_1 "]+?:?"
    "[" COMMON_1 "]++@"
    ")?+";
static const char host[] = "(?:[" COMMON_1 "]*+)"; // www.foo.bar
static const char port[] = "(?::[0-9]+)?+"; // :1234

#define COMMON_2 "a-z0-9\\-._~%!$&'()*+,;=:@/"
static const char path[] = "(?:/[" COMMON_2 "]+)?+"; // /path/to/some/place
static const char query[] = "(?:\\?[" COMMON_2 "]+)?+"; // "?somequery=bar"
static const char fragment[] = "(?:#[" COMMON_2 "]+)?+"; // "#fragment"

using LS1 = QLatin1String;

const QRegularExpression UrlFilter::FullUrlRegExp(
    LS1(scheme_or_www)
    + LS1(userInfo)
    + LS1(host)
    + LS1(port)
    + LS1(path)
    + LS1(query)
    + LS1(fragment)
    , QRegularExpression::CaseInsensitiveOption);


/////////////////////////////////////////////

// email address:
// [word chars, dots or dashes]@[word chars, dots or dashes].[word chars]
const QRegularExpression UrlFilter::EmailAddressRegExp(QStringLiteral("\\b(\\w|\\.|-|\\+)+@(\\w|\\.|-)+\\.\\w+\\b"));

// matches full url or email address
const QRegularExpression UrlFilter::CompleteUrlRegExp(
    QLatin1Char('(') + FullUrlRegExp.pattern() + QLatin1Char('|') + EmailAddressRegExp.pattern()+ QLatin1Char(')'),
    QRegularExpression::CaseInsensitiveOption);

/* clang-format on */
UrlFilter::UrlFilter()
{
    setRegExp(CompleteUrlRegExp);
}

QSharedPointer<HotSpot> UrlFilter::newHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts)
{
    return QSharedPointer<HotSpot>(new UrlFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts));
}
