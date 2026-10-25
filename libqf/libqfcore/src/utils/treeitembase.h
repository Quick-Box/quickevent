
//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2006
//
// Copyright: See COPYING file that comes with this distribution
//

#ifndef QF_CORE_UTILS_TREEITEMBASE_H
#define QF_CORE_UTILS_TREEITEMBASE_H

#include "../core/coreglobal.h"

#include <QList>
#include <QMetaType>

namespace qf {
namespace core {
namespace utils {
	
class TreeItemPath : public QList<int>
{
	public:
		static QFCORE_DECL_EXPORT void ensureMetatypeRegistered();
		static QFCORE_DECL_EXPORT const TreeItemPath& invalidPath();
		static QFCORE_DECL_EXPORT const TreeItemPath& rootPath();
	public:
		QFCORE_DECL_EXPORT int length() const;
		int count() const {return length();}
		QFCORE_DECL_EXPORT bool isValid() const;
		QFCORE_DECL_EXPORT QString toString() const;
		static QFCORE_DECL_EXPORT TreeItemPath fromString(const QString &path_str);
	public:
		/// invalid path
		/**
		Invalid path is a list with one entry. This entry shall be -1.
		*/
		TreeItemPath() : QList< int >() {*this = invalidPath();}
		TreeItemPath(const QList<int> &o) : QList< int >(o) {};
};


class QFCORE_DECL_EXPORT TreeItemBase
{
	public:
		TreeItemBase(TreeItemBase *parent);
		virtual ~TreeItemBase();
	public:
		QList<TreeItemBase*>& childrenRef() {return f_children;}
		const QList<TreeItemBase*>& children() const {return f_children;}
		int childrenCount() const {return children().count();}
		virtual TreeItemBase* parent() const {return f_parent;}
		template <typename T> T findParent() const
		{
			TreeItemBase *p = parent();
			while(p) {
				T pt = dynamic_cast<T>(p);
				if(pt) return pt;
				p = p->parent();
			}
			return nullptr;
		}
		/// if ix < 0, than index is counted from end
		virtual TreeItemBase* child(int ix) const;
		//TreeItemBase* lastChild() const {return child(-1);}
		int indexOfChild(TreeItemBase *it_child) const {return f_children.indexOf(it_child);}
		TreeItemPath path() const;
		virtual TreeItemBase* cd(const TreeItemPath &path) const;
		TreeItemBase* sibbling(int offset = 1);
		void shift(bool up = true, bool wrap = true);
		virtual void insertChild(int before_ix, TreeItemBase *it_child);
		//! vymeni soucasne dite na indexu \a child_ix za \a it_child a vrati puvodni dite
		virtual TreeItemBase* exchangeChild(int child_ix, TreeItemBase *it_child);
		void exchangeChild(TreeItemBase *it_old_child, TreeItemBase *it_new_child);
		void  setParent(TreeItemBase *_parent);
		virtual void clearChildren();
		void unlinkChild(TreeItemBase *child);
	protected:
		TreeItemBase *f_parent;
		QList<TreeItemBase*> f_children;
};

}}}

Q_DECLARE_METATYPE(qf::core::utils::TreeItemPath)

#endif // QF_CORE_UTILS_TREEITEMBASE_H

