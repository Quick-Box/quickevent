import QtQml 2.0
import qf.core 1.0
import "private"

QtObject {
	property string name
	property FieldType type
	property int length: -1
	property var defaultValue: null
	property bool notNull: false
	property string comment
	property string generatedAs

	function createSqlScript(options)
	{
		var driver_name = options.driverName;
		var def = '\t' + name + ' ' + type.createSqlScript(options);
		if(notNull)
			def += ' NOT NULL';
		if(defaultValue !== null) {
			def += ' DEFAULT ';
			if(typeof defaultValue === 'boolean' && driver_name.endsWith("SQLITE"))
				def += (defaultValue)? 1: 0;
			else if(defaultValue === 'CURRENT_TIMESTAMP')
				def += defaultValue;
			else if(typeof defaultValue === 'string')
				def += "'" + defaultValue + "'";
			else
				def += defaultValue;
		}
		if (generatedAs) {
			def += ' GENERATED ALWAYS AS (';
			def += generatedAs;
			def += ') STORED';
		}
		return def;
	}

	function createTypesSqlScript(options)
	{
		//Log.info("type:", type, typeof type.createSqlType, "ahoj:", type.ahoj, type.ahoj())
		var ret = type.createSqlType(options);
		return ret;
	}
}
