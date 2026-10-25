#ifndef QFCOREGLOBAL_H
#define	QFCOREGLOBAL_H

#include <qglobal.h>
/*
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	#if defined Q_CC_GNU && defined Q_OS_WIN32
		#ifndef Q_CC_MINGW
			#define Q_CC_MINGW
		#endif
		#ifndef WINVER
			#define WINVER 0x0501 /// pro mingw i win32 API jako, ze to je pro XP a vys
		#endif
	#endif
#endif
*/

#define QF_FUNC_NAME Q_FUNC_INFO

/// Declaration of macros required for exporting symbols
/// into shared libraries
#if defined(QFCORE_BUILD_DLL)
//#warning "EXPORT"
#  define QFCORE_DECL_EXPORT Q_DECL_EXPORT
#else
//#warning "IMPORT"
#  define QFCORE_DECL_EXPORT Q_DECL_IMPORT
#endif

#ifdef Q_CC_MSVC
/// MSVC exports the Qt container base of every exported class derived from it, which causes duplicate symbols
/// when more DLLs do so. Export the instances from libqfcore only, others import them.
/// libqfcore itself must not declare it extern, dllexport and extern are incompatible (C4910).
#include <QVariantMap>
#ifndef QFCORE_BUILD_DLL
extern template class QFCORE_DECL_EXPORT QMap<QString, QVariant>;
#endif
#endif

#endif // QFCOREGLOBAL_H
