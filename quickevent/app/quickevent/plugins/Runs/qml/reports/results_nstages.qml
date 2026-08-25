import QtQml 2.0
import qf.core 1.0
import qf.qmlreports 1.0
import shared.qml.reports 1.0
import "qrc:/quickevent/core/js/ogtime.js" as OGTime

Report {
	id: root

	property var options
	property var eventConfig
	property var stageConfig
	property int stageId
	property bool isBreakAfterEachClass: options.isBreakAfterEachClass? true: false
	property bool isColumnBreak: options.isColumnBreak? true: false
	property int stagesCount: (options.stagesCount > 0)? options.stagesCount: 1

	property string reportTitle: qsTr("Results after %n stage(s)", "", root.stagesCount)
	property int timeCellWidth: OGTime.timeMsColumnWidth(eventConfig.timeMeasurementPrecision)
	property int posCellWidth: 9
	property int lossCellWidth: root.timeCellWidth + 2
	property int unrealTimeMs: OGTime.UNREAL_TIME_MSEC

	property QfObject internals: QfObject {
		Component {
			id: cHeaderCell
			Cell {
				textStyle: myStyle.textStyleBold
			}
		}
		Component {
			id: cTimeCell
			Cell {
				property string fieldName
				property string invalidTimeString: "-----"
				textFn: function() {
					var time_ms = runnersDetail.data(runnersDetail.currentIndex, fieldName);
					if(time_ms < unrealTimeMs)
						return OGTime.msecToString_mmss(time_ms);
					return invalidTimeString;
				}
			}
		}
		Component {
			id: cTimeLossCell
			Cell {
				property string fieldName
				textFn: function() {
					var loss_ms = runnersDetail.data(runnersDetail.currentIndex, fieldName);
					if(loss_ms < unrealTimeMs)
						return "+" + OGTime.msecToString_mmss(loss_ms);
					return "";
				}
			}
		}
		Component {
			id: cPosCell
			Cell {
				property string fieldName
				textFn: function() {
					var pos = runnersDetail.data(runnersDetail.currentIndex, fieldName);
					return pos? "(" + pos + ")": ""
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
					Break {
						breakType: root.isColumnBreak? Break.Column: Break.Page;
						visible: root.isBreakAfterEachClass;
						skipFirst: true
					}
					Space { height: 5 }
					Frame {
						id: classHeader
						width: "%"
						layout: Frame.LayoutHorizontal
						fill: Brush {color: Color {def: "khaki"} }
						Cell {
							width: "%"
							textFn: detail.dataFn("classes.name");
							textStyle: myStyle.textStyleBold
						}
						Cell {
							id: hdrRegistration
							width: 17
							textStyle: myStyle.textStyleBold
							text: qsTr("Reg");
						}
						Component.onCompleted: {
							//console.warn("=============", root.stageCount)
							for(var i=0; i<root.stagesCount; i++) {
								var c = cHeaderCell.createObject(null, {"halign": Frame.AlignRight, "width": timeCellWidth + posCellWidth, "text": qsTr("Stage ") + (i+1)});
								classHeader.addItem(c);
							}
							c = cHeaderCell.createObject(null, {"halign": Frame.AlignRight, "width": timeCellWidth, "text": qsTr("Time")});
							classHeader.addItem(c);
							c = cHeaderCell.createObject(null, {"halign": Frame.AlignRight, "width": lossCellWidth, "text": qsTr("Loss")});
							classHeader.addItem(c);
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
								width: posCellWidth
								halign: Frame.AlignRight
								text: runnersDetail.data(runnersDetail.currentIndex, "pos");
							}
							Cell {
								width: "%"
								textFn: runnersDetail.dataFn("competitorName");
							}
							Cell {
								width: hdrRegistration.width
								textFn: runnersDetail.dataFn("registration");
							}
							Component.onCompleted: {
								//console.warn("=============", root.stageCount)
								for(var i=0; i<root.stagesCount; i++) {
									var c = cTimeCell.createObject(null, {"width": timeCellWidth, "halign": Frame.AlignRight, "fieldName": "timeMs" + (i+1)});
									runnersDetail.addItem(c);
									c = cPosCell.createObject(null, {"width": posCellWidth, "halign": Frame.AlignRight, "fieldName": "pos" + (i+1)});
									runnersDetail.addItem(c);
								}
								c = cTimeCell.createObject(null, {"invalidTimeString": qsTr("DISQ"), "width": timeCellWidth, "halign": Frame.AlignRight, "fieldName": "timeMs"});
								runnersDetail.addItem(c);
								c = cTimeLossCell.createObject(null, {"width": lossCellWidth, "halign": Frame.AlignRight, "fieldName": "timeLossMs"});
								runnersDetail.addItem(c);
							}
						}
					}
				}
			}
		}
	}
}
