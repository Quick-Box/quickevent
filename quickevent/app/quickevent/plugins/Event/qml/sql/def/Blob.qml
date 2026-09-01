import "private"

FieldType
{
	function createSqlScript(options)
	{
		var def;
		if(options.driverName.endsWith("PSQL")) {
			def = "BYTEA";
		} else {
			def = "BLOB";
		}
		return def;
	}

	function metaTypeNameFn()
	{
		return "QByteArray";
	}
}
