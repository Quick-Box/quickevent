import qf.qmlreports 1.0
import shared.qml.reports 1.0

Report {
	id: root
	objectName: "root"

	property string reportTitle: qsTr("Start list by classes")
	property var options
	property var eventConfig
	property var stageConfig
	property int stageId
	property bool isBreakAfterEachClass: options.isBreakAfterEachClass? true: false
	property bool isColumnBreak: options.isColumnBreak? true: false
	property bool isPrintStartNumbers: options.isStartListPrintStartNumbers? true: false

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
					Frame {
						width: "%"
						layout: Frame.LayoutHorizontal
						fill: Brush {color: Color {def: "khaki"} }
						Cell {
							width: "%"
							textFn: detail.dataFn("classes.name");
							textStyle: myStyle.textStyleBold
						}
						Cell {
							textFn: function() { return qsTr("length: ") + detail.rowData("courses.length");}
						}
						Cell {
							textFn: function() { return qsTr("climb: ") + detail.rowData("courses.climb");}
						}
					}
					//expandChildFrames: true
					Band {
						id: runnersBand
						objectName: "runnersBand"
						keepFirst: 3
						keepWithPrev: true
						htmlExportAsTable: true
						Detail {
							id: runnersDetail
							objectName: "runnersDetail"
							width: "%"
							layout: Frame.LayoutHorizontal
							function dataFn(field_name) {return function() {return rowData(field_name);}}
							Cell {
								width: 13
								halign: Frame.AlignRight
								textFn: runnersDetail.dataFn("startTimeText");
							}
							Para {
								visible: root.isPrintStartNumbers
								width: 8
								halign: Frame.AlignRight
								textFn: function() {
									var sn = runnersDetail.dataFn("startNumber")();
									return sn > 0? sn: "";
								}
							}
							Cell {
								width: "%"
								textFn: runnersDetail.dataFn("competitorName");
							}
							Para {
								width: 16
								textFn: runnersDetail.dataFn("registration");
							}
							Cell {
								width: 17
								halign: Frame.AlignRight
								textFn: function() {
									var si = runnersDetail.dataFn("runs.siId")();
									return si > 0 ? si : "";
								}
							}
						}
					}
				}
			}
		}
	}
}


