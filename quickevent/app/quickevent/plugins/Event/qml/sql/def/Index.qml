import QtQml 2.0
import qf.core 1.0
//import qf.core.sql.def 1.0

QtObject
{
	property string name
	property var fields: []
	property bool primary: false
	property bool unique: false
	property ForeignKeyReference references: null
	property string where
	property string comment

	function indexName(cnt, table_name)
	{
		var ret = name;
		if(!ret) {
			if(references) {
				ret = table_name + '_foreign' + cnt;
			}
			else if(primary) {
				ret = table_name + '_pkey';
			}
			else if(unique) {
				ret = table_name + '_unique' + cnt;
			}
			else {
				ret = table_name + '_ix' + cnt;
			}
		}
		return ret;
	}

	function createSqlConstraintScript(constr_no, options)
	{
		var ret = '';
		var full_table_name = options.tableName;
		if(options.driverName.endsWith("PSQL"))
			full_table_name = options.schemaName + '.' + full_table_name;
		if(fields) {
			if(references) {
				var full_referenced_table_name = references.table;
				if(options.driverName.endsWith("PSQL"))
					full_referenced_table_name = options.schemaName + '.' + full_referenced_table_name;
				ret += '\tCONSTRAINT ' + indexName(constr_no, options.tableName)
					+ ' FOREIGN KEY ('
					+ fields.join(', ')
					+ ') REFERENCES '
					+ full_referenced_table_name + ' ('
					+ references.fields.join(', ')
					+ ')'
					+ ' ON UPDATE ' + references.onUpdate
					+ ' ON DELETE ' + references.onDelete;
			}
			else if(primary) {
				ret += '\tCONSTRAINT ' + indexName(constr_no, options.tableName) + ' PRIMARY KEY (' + fields.join(', ') + ')';
			}
			else if(unique && !where) {
				ret += '\tCONSTRAINT ' + indexName(constr_no, options.tableName) + ' UNIQUE (' + fields.join(', ') + ')';
			}
		}
		return ret;
	}

	function createSqlIndexScript(index_name, table_name, options)
	{
		if(fields) {
			if (where) {
				return 'CREATE '
						+ (unique? 'UNIQUE ': ' ')
						+'INDEX ' + index_name
						+ ' ON ' + table_name + ' (' + fields.join(', ') + ')'
						+ ' WHERE ' + where;
			}
			else if(!primary && !unique && !references) {
				return 'CREATE INDEX ' + index_name
						+ ' ON ' + table_name
						+ ' (' + fields.join(', ') + ')';
			}
		}
		return '';
	}
}
