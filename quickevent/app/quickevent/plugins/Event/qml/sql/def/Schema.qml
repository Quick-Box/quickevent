import QtQml 2.0
import qf.core 1.0
import "qrc:/qf/core/qml/js/stringext.js" as StringExt

QtObject {
	id: root
	//property string name
	property list<Table> tables
	property list<Insert> inserts

	function createSqlScript(options)
	{
		// console.debug("createSqlScript options:", JSON.stringify(options, null, 2));
		//if(options.schemaName)
		//	name = options.schemaName;
		var driver_name = options.driverName;
		var ret = [];

		ret.push('------------------------------------');
		ret.push('-- create schema ' + options.schemaName);
		ret.push('------------------------------------');
		if(driver_name.endsWith("PSQL"))
			ret.push('CREATE SCHEMA ' + options.schemaName);
		for(var i=0; i<tables.length; i++) {
			ret.push('');
			ret.push.apply(ret, tables[i].createSqlScript(options));
		}

		ret.push('');
		ret.push('------------------------------------');
		ret.push('-- insert initial data');
		ret.push('------------------------------------');
		for(var i=0; i<inserts.length; i++) {
			ret.push('');
			ret.push.apply(ret, inserts[i].createSqlScript(options));
		}
		return ret;//.join(';\n');
	}
}
