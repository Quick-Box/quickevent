import QtQml 2.0
import qf.core 1.0
import qf.qmlreports 1.0
import shared.qml.reports 1.0

Report {
	id: root

	property var options
	property var eventConfig
	property var stageConfig
	property int stageId
	property bool isBreakAfterEachClass: options.isBreakAfterEachClass? true: false
	property bool isColumnBreak: options.isColumnBreak? true: false
	property bool isPrintStartNumbers: options.isStartListPrintStartNumbers? true: false
	property int stagesCount: (options.stagesCount > 0)? options.stagesCount: 1
	property string reportTitle: qsTr("Start list by clubs for %n stage(s)", "", root.stagesCount)

	property QfObject internals: QfObject {
		Component {
			id: cHeaderCell
			Cell {
				textStyle: myStyle.textStyleBold
			}
		}
		Component {
			id: cStartTimeCell
			Cell {
				property string fieldName
				property string isRunning
				textFn: function() {
					var run = runnersDetail.data(runnersDetail.currentIndex, isRunning);
					if (run)
						return runnersDetail.data(runnersDetail.currentIndex, fieldName);
					else
						return "-";
				}
			}
		}
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
			//columns: "%,%"
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
				Detail {
					id: detail
					objectName: "detail"
					width: "%"
					//keepAll: true
					layout: Frame.LayoutVertical
					function dataFn(field_name) {return function() {return rowData(field_name);}}
					Space { height: 5 }
					Break {
						breakType: root.isColumnBreak? Break.Column: Break.Page;
						visible: root.isBreakAfterEachClass;
						skipFirst: true
					}
					Frame {
						id: classHeader
						width: "%"
						layout: Frame.LayoutHorizontal
						fill: Brush {color: Color {def: "khaki"} }
						Cell {
							textFn: detail.dataFn("clubAbbr");
							textStyle: myStyle.textStyleBold
						}
						Cell {
							width: "%"
							textFn: detail.dataFn("name");
							textStyle: myStyle.textStyleBold
						}
						Cell {
							visible: root.isPrintStartNumbers
							id: hdrStartNumber
							width: 12
							textStyle: myStyle.textStyleBold
							halign: Frame.AlignRight
							text: qsTr("Bib");
						}
						Cell {
							id: hdrRegistration
							width: 23
							textStyle: myStyle.textStyleBold
							text: qsTr("Registration");
						}
						Cell {
							id: hdrSI
							width: 19
							textStyle: myStyle.textStyleBold
							halign: Frame.AlignRight
							text: qsTr("SI");
						}
						Component.onCompleted: {
							//console.warn("============= root.stageCount:", root.stageCount)
							for(var i=0; i<root.stagesCount; i++) {
								//console.warn("=============", i, qsTr("Stage") + (i+1))
								//var runs_table = "runs" + (i+1);
								var c = cHeaderCell.createObject(null, {"halign": Frame.AlignRight, "width": 17, "text": qsTr("Stage") + (i+1)});
								classHeader.addItem(c);
							}
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
								width: 12
								textFn: runnersDetail.dataFn("classes.name");
							}
							Cell {
								width: "%"
								textFn: runnersDetail.dataFn("competitorName");
							}
							Cell {
								visible: root.isPrintStartNumbers
								width: hdrStartNumber.width
								halign: Frame.AlignRight
								textFn: function() {
									var sn = runnersDetail.dataFn("startNumber")();
									return sn > 0? sn: "";
								}
							}
							Cell {
								width: hdrRegistration.width
								textFn: runnersDetail.dataFn("registration");
							}
							Cell {
								width: hdrSI.width
								halign: Frame.AlignRight
								textFn: runnersDetail.dataFn("competitors.siId");
							}
							Component.onCompleted: {
								//console.warn("=============", root.stageCount)
								for(var i=0; i<root.stagesCount; i++) {
									var runs_table = "runs" + (i+1);
									var c = cStartTimeCell.createObject(null, {"width": 17, "halign": Frame.AlignRight, "fieldName": runs_table + ".startTimeText", "isRunning": runs_table + ".isRunning" });
									runnersDetail.addItem(c);
								}
							}
						}
					}
				}
			}
		}
	}
}


