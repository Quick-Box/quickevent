import qf.qmlreports 1.0
import shared.qml.reports 1.0
import "qrc:/quickevent/core/js/ogtime.js" as OGTime

Report {
	id: root
	objectName: "root"

	property string reportTitle: qsTr("Results")
	property bool isBreakAfterEachClass: false
	property bool isColumnBreak: false
	property bool isPrintStartNumbers: false
	property var options
	property var eventConfig
	property var stageConfig
	property int stageId

	function ogSumTimeToString(time_ms)
	{
		//return "999.99";
		if(time_ms)
			return OGTime.msecToString_mmss(time_ms);
		return "";
	}

	function ogLossToString(time_ms)
	{
		//return "999.99";
		if(time_ms)
			return '+' + OGTime.msecToString_mmss(time_ms);
		return "";
	}

	function posToStringDot(pos)
	{
		//return "999.";
		if(pos)
			return pos + '.';
		return '';
	}

	function posToStringBrackets(pos)
	{
		//return "(999)";
		if(pos)
			return '(' + pos + ')';
		return '';
	}

	//debugLevel: 1
	styleSheet: StyleSheet {
		objectName: "portraitStyleSheet"
		basedOn: ReportStyleCommon { id: myStyle }
		colors: [
		]
		pens: [
			Pen {name: "red1dot"
				basedOn: "black1"
				color: Color {def:"red"}
				style: Pen.DotLine
			},
			Pen {
				id: pen_black1
				basedOn: "black1"
			}
		]
	}
	textStyle: myStyle.textStyleDefault

	width: root.options.pageWidth? root.options.pageWidth: 210
	height: root.options.pageHeight? root.options.pageHeight: 297
	hinset: root.options.horizontalMargin? root.options.horizontalMargin: 10
	vinset: root.options.verticalMargin? root.options.verticalMargin: 5
	Frame {
		width: "%"
		height: "%"
		layout: Frame.LayoutStacked
		QuickEventHeaderFooter {
			reportTitle: root.reportTitle
		}
		Frame {
			width: "%"
			height: "%"
			columns: root.options.columns
			vinset: 10
			Band {
				id: band
				objectName: "band"
				width: "%"
				height: "%"
				QuickEventReportHeader {
					eventConfig: root.eventConfig
					stageConfig: root.stageConfig
					stageId: root.stageId
					reportTitle: root.reportTitle
				}
				Space { height: 5 }
				Detail {
					id: detail
					objectName: "detail"
					width: "%"
					//keepAll: true
					layout: Frame.LayoutVertical
					function dataFn(field_name) {return function() {return rowData(field_name);}}
					Break {
						breakType: root.isColumnBreak? Break.Column: Break.Page;
						visible: root.isBreakAfterEachClass;
						skipFirst: true
					}
					Space {height: 2}
					Frame {
						width: "%"
						vinset: 1
						layout: Frame.LayoutHorizontal
						fill: Brush {color: Color {def: "khaki"} }
						Cell {
							width: "%"
							textFn: detail.dataFn("className");
							textStyle: myStyle.textStyleBold
						}
						//Cell {
						//	textFn: function() { return qsTr("length: ") + detail.rowData("courses.length");}
						//}
					}
					//expandChildFrames: true
					Band {
						id: relayBand
						width: "%"
						//objectName: "relayBand"
						//keepFirst: 3
						//keepWithPrev: true
						htmlExportAsTable: true
						Detail {
							id: relayDetail
							//objectName: "runnersDetail"
							width: "%"
							layout: Frame.LayoutVertical
							function dataFn(field_name) {return function() {return rowData(field_name);}}
							//Space {height: 2}
							Frame {
								width: "%"
								layout: Frame.LayoutHorizontal
								valign: Frame.AlignBottom
								//bottomBorder: Pen { basedOn: "black1" }
								//fill: Brush {color: Color {def: "khaki"} }
								//textStyle: myStyle.textStyleBold
								Para {
									width: 9
									halign: Frame.AlignRight
									textFn: function() {return root.posToStringDot(relayDetail.rowData("pos"))}
								}
								Cell {
									width: "%"
									textFn: function() { return relayDetail.rowData("name"); }
								}
								Para {
									width: 15
									halign: Frame.AlignRight
									textFn: function() {return OGTime.ogTimeToString(relayDetail.rowData("time"))}
								}
								Para {
									width: 17
									halign: Frame.AlignRight
									textFn: function() {return root.ogLossToString(relayDetail.rowData("loss"))}
								}
							}
						}
					}
				}
			}
		}
	}
}


