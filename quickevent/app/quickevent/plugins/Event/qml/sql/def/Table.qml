import QtQml 2.0
import qf.core 1.0
import "private/libsqldef.js" as LibSqlDef

QtObject {
	id: root
	property string name
	property list<Field> fields
	property list<Index> indexes
	property string comment

	function createSqlScript(options)
	{
		var opts = new LibSqlDef.Options(options);
		var ret = [];
		var sql_types = [];
		var full_table_name = opts.fullTableName(name);
		ret.push('-- create table: ' + full_table_name);
		var table_def = 'CREATE TABLE ' + full_table_name + ' (\n';
		var field_defs = [];
		options.tableName = name;
		var fieldNames = []
		for(var field_ix=0; field_ix<fields.length; field_ix++) {
			var field = fields[field_ix];
			fieldNames.push(field.name);
			//Log.info("field:", field);
			field_defs.push(field.createSqlScript(options));
			var sql_type = field.createTypesSqlScript(options);
			if(sql_type)
				sql_types.push(sql_type);
		}
		for(var constraint_ix=0; constraint_ix<indexes.length; constraint_ix++) {
			var constr = indexes[constraint_ix].createSqlConstraintScript(constraint_ix, options);
			if(constr)
				field_defs.push(constr);
		}
		table_def += field_defs.join(',\n');
		table_def += '\n)';

		ret.push.apply(ret, sql_types);

		ret.push(table_def);

		for(var index_ix=0; index_ix<indexes.length; index_ix++) {
			var index = indexes[index_ix];
			var index_def = index.createSqlIndexScript(index.indexName(index_ix, name), full_table_name, options);
			if(index_def)
				ret.push(index_def);
		}
		var comments_prefix = "";
		if(!opts.isTableCommentsSupported()) {
			comments_prefix = "-- comments not suported for driver: " + options.driverName + "\n-- ";
		}
		for(var comment_ix=0; comment_ix<fields.length; comment_ix++) {
			var fld = fields[comment_ix];
			if(fld.comment) {
				ret.push(comments_prefix + 'COMMENT ON COLUMN ' + full_table_name + '.' + fld.name + " IS '" + fld.comment + "'");
			}
		}
		if(root.comment) {
			ret.push(comments_prefix + 'COMMENT ON TABLE ' + full_table_name + " IS '" + root.comment + "'");
		}
		return ret;
	}
}
