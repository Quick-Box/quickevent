# Reports

QuickEvent reports (start lists, results, receipts, ...) are defined in XML files
loaded by `qf::gui::reports::XmlReportDocument`
(`libqf/libqfgui/src/reports/processor/xmlreportdocument.h`).

Report files are in `quickevent/app/quickevent/plugins/*/qml/reports/`, shared parts
(style sheet, page header and footer) are in `plugins/shared/qml/reports/`.

## Basic structure

* Element names are report item and style types.
* Attributes are properties of the item.
* Enum values are written as enum key names, for example `layout="LayoutHorizontal"`.
* The root element must be `Report`.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Report width="210" height="297" hinset="10" vinset="5" textStyle="default">
	<Parameter name="reportTitle" default="${'Start list by classes'|tr}"/>
	<styleSheet><include src="../../../shared/qml/reports/ReportStyleCommon.xml"/></styleSheet>
	<Frame width="%" height="%" layout="LayoutStacked">
		<include src="../../../shared/qml/reports/QuickEventHeaderFooter.xml"/>
		<Band width="%" height="%">
			<Detail width="%">
				<Frame width="%" layout="LayoutHorizontal" fill="khaki">
					<Para width="%" textStyle="bold">{{classes.name}}</Para>
				</Frame>
				<Band keepFirst="3" keepWithPrev="true">
					<Detail width="%" layout="LayoutHorizontal">
						<Para width="15" halign="AlignRight">{{startTimeText}}</Para>
						<Para width="%">{{competitorName}}</Para>
						<Para width="17" halign="AlignRight">{{runs.siId|positive}}</Para>
					</Detail>
				</Band>
			</Detail>
		</Band>
	</Frame>
</Report>
```

## Report items

| Element  | Description |
|----------|-------------|
| `Report` | Root element. `width` and `height` are the page size in mm. |
| `Frame`  | Container that lays out its children. |
| `Space`  | Same as `Frame`, used as an empty spacer. |
| `Band`   | Frame bound to a data table. |
| `Detail` | Frame printed once for each row of the parent `Band` table. |
| `Para`   | Text paragraph. The element text is the paragraph text. Only `Para` can contain text. |
| `Image`  | Image loaded from `dataSource` (file path) or `data`. |
| `Break`  | Column or page break. |

### Common properties

All items:

* `visible` – `false` hides the item.
* `keepWithPrev` – keeps the item on the same page as the previous item.
* `keepAll` – the item is not split across pages.
* `htmlExportAttributes` – JSON map of attributes used in the HTML export, for example
  `htmlExportAttributes='{"lpt_textWidth": "4"}'`.

Frames (and also `Report`, `Band`, `Detail`, `Para` and `Image`):

* `width`, `height` – size in mm. The `%` value takes the remaining space and `50%` takes a
  proportional part of it. If the size is not set, the item is as big as its content.
* `hinset`, `vinset` – horizontal and vertical inner margin in mm.
* `layout` – `LayoutVertical` (default), `LayoutHorizontal` or `LayoutStacked` (children are drawn over each other).
* `halign` – `AlignLeft`, `AlignRight`, `AlignHCenter` or `AlignJustify`.
* `valign` – `AlignTop`, `AlignBottom` or `AlignVCenter`.
* `columns` – column widths, for example `columns="%,%"` for two equal columns. `columnsGap` is the gap between columns in mm.
* `border`, `topBorder`, `bottomBorder`, `leftBorder`, `rightBorder` – pen.
* `fill` – brush.
* `textStyle` – text style for this frame and all its children.
* `expandVerticalSprings`, `expandChildFrames` – layout flags.

`Band`:

* `modelData` – data table name. In a nested band, it is the name of a child table of the parent
  `Detail`'s current row. In the top band, it is a key of the data passed to the report processor.
* `keepFirst` – the number of first detail rows that are kept together with the band header.
* `headerOnBreak` – prints the band header again after a page break.
* `createFromData` – creates child items from data.
* `htmlExportAsTable` – exports the band as an HTML table.

`Para`:

* `text` – can be used instead of the element text.
* `omitEmptyText` (default `true`), `textWrap` (default `true`).
* `textHAlign`, `textVAlign` – text alignment.

`Image`:

* `dataSource`, `data`, `dataFormat`, `dataEncoding`, `dataCompression`.
* `aspectRatio` – default is `AspectRatioKeep`.
* `suppressPrintout` – the image is not printed.

`Break`:

* `breakType` – `Column` (default) or `Page`.
* `skipFirst` – the first break is not done, for example to avoid a break before the first class.

## Styles

Styles are defined in a `StyleSheet` set as the report's `styleSheet` property. Every style object has a `name`,
and it can be based on another object with the same type using `basedOn`.

```xml
<StyleSheet>
	<Color name="khaki" def="khaki"/>
	<Pen name="black1" color="black" width="1"/>
	<Pen name="black1dot" basedOn="black1" style="DotLine"/>
	<Brush name="khaki" color="khaki"/>
	<Font name="default" pointSize="9"/>
	<Font name="bold" basedOn="default" weight="WeightBold"/>
	<Font name="strikeOut" basedOn="default" strikeOut="StrikeOutTrue"/>
	<TextStyle name="default" font="default"/>
	<TextStyle name="bold" font="bold"/>
	<TextStyle name="reportHeader" font="italic" pen="blue1"/>
</StyleSheet>
```

| Element     | Properties |
|-------------|------------|
| `Color`     | `def` – color name or `#rrggbb` / `#aarrggbb` |
| `Pen`       | `color`, `width`, `style` (`SolidLine`, `DashLine`, `DotLine`, ...) |
| `Brush`     | `color`, `style` |
| `Font`      | `family`, `pointSize`, `weight` (`WeightBold`, ...), `style` (`StyleItalic`, ...), `hint`, `strikeOut` |
| `TextStyle` | `font`, `pen` |

You can set a style property (`textStyle`, `border`, `fill`, `font`, `pen`, `color`, ...) in two ways:

* As an attribute. The value is the name of a style object from the report style sheet, and the
  new object is based on that one, for example `fill="khaki"`.
* As a property element with a full object definition:

  ```xml
  <Frame>
  	<bottomBorder><Pen color="blue" width="0.5"/></bottomBorder>
  </Frame>
  ```

An empty attribute value sets no style object, so the style is inherited from the parent.
This is useful in print time expressions.

## Special elements

### Parameter

```xml
<Parameter name="reportTitle" default="${'Results'|tr}"/>
```

Defines report parameter `reportTitle`. The `default` value is used if the parameter is not
passed to the report. Report parameters are the report init properties passed to
`ReportProcessor::setReport()`.

### include

```xml
<include src="../../../shared/qml/reports/QuickEventHeaderFooter.xml" width="%" reportTitle="Results"/>
```

Includes the root element of another report file. The `src` path is relative to the current file.
Attributes that are properties of the included root element override those properties.
Other attributes are passed as parameters to the included file.

### Repeat

```xml
<Repeat var="i" from="1" to="${stagesCount}">
	<Para width="18">${'Stage'|tr} ${i}</Para>
</Repeat>
```

Creates its child elements repeatedly, with parameter `i` set to `from`, `from + 1`, ..., `to`.
`var` defaults to `i` and `from` defaults to `1`.

## Expressions

There are two kinds of expressions:

* `${expression}` – evaluated when the report is loaded. It can be used in any attribute or text.
  Use it for parameters, options and translations of static texts.
* `{{expression}}` – evaluated when the report is printed. It can be used in `Para` text and in report item
  properties, for example `textStyle="{{dropped|if:strikeOut:bold}}"`.

If an attribute consists of a single expression, the value keeps its type (number, bool, ...).
Otherwise the expression results are converted to strings and joined with the surrounding text.

An expression is a value followed by optional formatters:

```
value|formatter|formatter:argument
```

A formatter argument starting with `$` is a report parameter value, for example `add:$width`.

### Values

| Value | Description |
|-------|-------------|
| `'text'`, `"text"` | string literal |
| `123` | number literal |
| `name(arg, ...)` | function call; arguments are expressions |
| `$name`, `$name.key` | report parameter; nested maps are accessed with dot notation. In `${}` the `$` is optional. |
| `field` | value of a field from the nearest `Detail` current row or the nearest `Band` data, `{{}}` only |
| `$page` | current page number, `{{}}` only |
| `$now` | current date-time, `{{}}` only |
| `$row`, `$rowCount`, `$lastRow` | current row index, row count and whether the current row is the last one of the nearest `Detail`, `{{}}` only |

In `{{}}`, an unknown function is taken as a field name. Data can contain aggregated fields like `SUM(field)`.

The total page count can be printed with `@{n}`, for example `Page {{$page}}/@{n}`.

### Functions

| Function | Description |
|----------|-------------|
| `select(condition, then, else)` | `then` if the condition is true, `else` otherwise |
| `coalesce(value, ...)` | first non-empty value |
| `add(a, ...)`, `sub(a, b)`, `mul(a, ...)`, `div(a, b)` | arithmetic; division by zero gives an empty value |
| `and(a, ...)`, `or(a, ...)`, `not(a)` | logic |
| `eq(a, b)`, `lt(a, b)`, `gt(a, b)` | comparison (`eq` compares strings, `lt` and `gt` compare numbers) |
| `band('name')` | value from the nearest `Band` data, useful to get band data inside a `Detail` |

Empty values, `false`, `0` and `'0'` are false. Everything else is true.

QuickEvent registers these additional functions (`quickevent/app/quickevent/src/reportfunctions.cpp`):

| Function | Description |
|----------|-------------|
| `ogRunStatus()` | run status text (`NC`, `DISQ`, `MP`, `DNS`, `DNF`, `OVRT`) from the current row flags |
| `qeStagePrefix()` | `E<stage> ` prefix of the report title for multi-stage events |
| `appVersion()` | application version |

### Formatters

| Formatter | Description |
|-----------|-------------|
| `tr` | translates the value. The translation context is the report file base name. |
| `trn:text` | translates `text` with plural forms; the value is the count (`%n` in text) |
| `date`, `time`, `datetime` | the optional argument is a `QDateTime` format string, for example `datetime:dd.MM.yyyy` |
| `number:decimals` | number with a fixed number of decimals |
| `add:number` | adds a number |
| `clamp:min:max` | limits the value to a range |
| `default:text` | `text` is used if the value is empty |
| `if:then:else` | `then` or `else` text, depending on the value |
| `not` | logical negation |
| `nonzero` | `0` becomes empty |
| `positive` | a number that is not positive becomes empty |
| `prefix:text`, `suffix:text` | added to non-empty values only |
| `key:name` | value of `name` key in a map |
| `arg:value` | replaces `%1` in the text with `value` |
| `changed` | the value if it differs from the value printed for the previous `Detail` row, empty otherwise |

QuickEvent formatters:

| Formatter | Description |
|-----------|-------------|
| `ogTime[:text]` | time in msec as `mm.ss`; `text` (if present) is used for unreal times |
| `ogTimeSec` | time in msec as `mm.ss` with second precision |
| `ogTimeStatus` | time in msec as `mm.ss`, or run status for special time values |
| `ogTimeColumnWidth` | time column width in mm for the time measurement precision |
| `hhmmss` | msec since midnight as `h:mm:ss` |
| `siTime` | SI card time in msec as `h:mm:ss`; a missing time is printed as `--:--:--` |

### Examples

```xml
<Report width="${options.pageWidth|nonzero|default:210}">
<Break breakType="${options.isColumnBreak|if:Column:Page}" visible="${options.isBreakAfterEachClass}"/>
<Para>{{'length: '|tr}}{{courses.length}}</Para>
<Para>{{div(band('courses.length'), 1000)|default:0}}km</Para>
<Para>{{select(runs${i}.isRunning, runs${i}.startTimeText, '-')}}</Para>
<Para>{{$now|datetime}}</Para>
<Para textStyle="{{dropped|if:strikeOut:bold}}">{{competitorName}}</Para>
```

## Extending

Custom functions and formatters can be registered from C++:

```cpp
using qf::gui::reports::XmlReportDocument;

// formatter with no argument gets null argument
XmlReportDocument::registerFormatter("upper", [](const QVariant &v, const QString &) -> QVariant {
	return v.toString().toUpper();
});
XmlReportDocument::registerFunction("fullName", [](const XmlReportDocument::ExpressionContext &ctx, const QVariantList &) -> QVariant {
	return ctx.field("firstName").toString() + ' ' + ctx.field("lastName").toString();
});
```

`ExpressionContext` provides `parameter(path)`, `field(name)` (available only during print), `item()`
(the item being printed, `nullptr` while the report is loading) and `isLoading()`.
