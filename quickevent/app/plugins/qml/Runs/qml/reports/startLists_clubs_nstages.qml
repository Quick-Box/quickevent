import QtQml 2.0
import qf.core 1.0
import qf.qmlreports 1.0
import shared.QuickEvent.reports 1.0
import "qrc:/quickevent/core/js/ogtime.js" as OGTime

Report {
	id: root

	property int stageCount: 1
	property string reportTitle: qsTr("Start list by clubs")

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
				textFn: function() {
					return OGTime.msecToString_mmss(runnersDetail.data(runnersDetail.currentIndex, fieldName));
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

	width: 210
	height: 297
	hinset: 5
	vinset: 5
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
					dataBand: band
					reportTitle: root.reportTitle
					showStageNumber: false
				}
				Detail {
					id: detail
					objectName: "detail"
					width: "%"
					//keepAll: true
					layout: Frame.LayoutVertical
					function dataFn(field_name) {return function() {return rowData(field_name);}}
					Space { height: 5 }
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
						Component.onCompleted: {
							//console.warn("=============", root.stageCount)
							for(var i=0; i<root.stageCount; i++) {
								var runs_table = "runs" + (i+1);
								var c = cHeaderCell.createObject(null, {"width": 15, "text": qsTr("Stage") + (i+1)});
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
								width: 20
								textFn: runnersDetail.dataFn("classes.name");
							}
							Cell {
								width: "%"
								textFn: runnersDetail.dataFn("competitorName");
							}
							Cell {
								width: 25
								textFn: runnersDetail.dataFn("registration");
							}
							Cell {
								width: 18
								halign: Frame.AlignRight
								textFn: runnersDetail.dataFn("competitors.siId");
							}
							Component.onCompleted: {
								//console.warn("=============", root.stageCount)
								for(var i=0; i<root.stageCount; i++) {
									var runs_table = "runs" + (i+1);
									var c = cStartTimeCell.createObject(null, {"width": 15, "halign": Frame.AlignRight, "fieldName": runs_table + ".startTimeMs"});
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


