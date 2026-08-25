import QtQml 2.0
import qf.core 1.0
import qf.qmlreports 1.0
import shared.qml.reports 1.0

Report {
	id: root
	objectName: "root"

	property var options
	property var eventConfig
	property var stageConfig
	property int stageId
	property bool isBreakAfterEachClass: options.isBreakAfterEachClass? true: false
	property bool isColumnBreak: options.isColumnBreak? true: false
	property int stagesCount: (options.stagesCount > 0)? options.stagesCount: 1

	property string reportTitle: qsTr("Points after %n stage(s)", "", root.stagesCount)

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
				QuickEventReportHeader {
					eventConfig: root.eventConfig
					stageConfig: root.stageConfig
					stageId: root.stageId
					reportTitle: root.reportTitle
					showStageNumber: false
				}
				Space { height: 5 }
				Detail {
					id: detail
					objectName: "detail"
					width: "%"
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
							width: 17
							halign: Frame.AlignRight
							textStyle: myStyle.textStyleBold
							text: qsTr("Reg")
						}
						Cell {
							width: 15
							halign: Frame.AlignRight
							textStyle: myStyle.textStyleBold
							text: qsTr("Result")
						}
						Cell {
							width: 15
							halign: Frame.AlignRight
							textStyle: myStyle.textStyleBold
							text: qsTr("Diff.")
						}
					}
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
								width: 9
								halign: Frame.AlignRight
								text: runnersDetail.data(runnersDetail.currentIndex, "pos");
							}
							Cell {
								width: "%"
								textFn: runnersDetail.dataFn("competitorName");
							}
							Cell {
								width: 17
								textFn: runnersDetail.dataFn("registration");
							}
							Cell {
								width: 15
								halign: Frame.AlignRight
								textFn: function() {
									var pts = runnersDetail.dataFn("points")();
									return (pts && pts > 0)? pts: "";
								}
							}
							Cell {
								width: 15
								halign: Frame.AlignRight
								textFn: function() {
									var pts = runnersDetail.dataFn("pointsloss")();
									return (pts === null || pts === undefined || pts === 0)? "": "- " + pts;
								}
							}
						}
					}
				}
			}
		}
	}
}
