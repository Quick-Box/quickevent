import QtQml 2.0
import qf.core 1.0
import qf.qmlreports 1.0
import shared.qml.reports 1.0
import "qrc:/quickevent/core/js/ogtime.js" as OGTime

Report {
	id: root
	property int stageCount: 1
	property string reportTitle: qsTr("Competitors statistics")

	property QfObject internals: QfObject {
		Component {
			id: cCell
			Cell { }
		}
		Component {
			id: cBandCell
			Cell {
				property string fieldName
				text: {var d = data(); return d? d: "";}
				function data() {
					return detail.data(detail.currentIndex, fieldName);
				}
			}
		}
		Component {
			id: cMapDiffCell
			Cell {
				property int stage
				property int diff: {
					var maps = data("e" + stage + "_mapCount");
					var runners = data("e" + stage + "_runCount");
					if(maps && runners)
						return maps - runners;
					return 0;
				}
				text: diff;
				fill: (diff < 0)? brushError: brushNone;
				function data(field_name) {
					return detail.data(detail.currentIndex, field_name);
				}
			}
		}
		Component {
			id: cSumCell
			Cell {
				property string fieldName
				textFn: function() {var d = data(); return d? d: "";}
				function data() {
					return band.data("SUM(" + fieldName + ")");
				}
			}
		}
	}

	//debugLevel: 1
	styleSheet: StyleSheet {
		basedOn: ReportStyleCommon {
			id: myStyle
		}
		brushes: [
			Brush {
				id: brushNone
				name: "none"
				color: Color {def:"#00000000"}
			},
			Brush {
				id: brushError
				name: "error"
				color: Color {def:"salmon"}
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
			vinset: 10
			Band {
				id: band
				objectName: "band"
				width: "%"
				height: "%"
				htmlExportAsTable: true
				Frame {
					Para {
						textStyle: TextStyle {basedOn: "big"}
						text: root.reportTitle;
					}
					Para {
						textStyle: myStyle.textStyleBold
						textFn: function() { var event_cfg = band.data("event"); return event_cfg.name; }
					}
					Para {
						textFn: function() { var event_cfg = band.data("event"); return event_cfg.date; }
					}
					Para {
						textFn: function() { var event_cfg = band.data("event"); return event_cfg.place; }
					}
				}
				Space { height: 5 }
				Frame {
					id: head
					layout: Frame.LayoutHorizontal
					textStyle: myStyle.textStyleBold
					bottomBorder: Pen { basedOn: "black1" }
					Cell {
						id: cellClassName
						width: 20
						text: qsTr("Class name")
					}
					Component.onCompleted: {
						console.debug("stageCount", root.stageCount)
						for(var i=0; i<root.stageCount; i++) {
							var c = cCell.createObject(null, {"text": "E" + (i+1), "width": 15, "halign": Frame.AlignRight});
							head.addItem(c);
							c = cCell.createObject(null, {"text": qsTr("maps"), "width": 15, "halign": Frame.AlignRight});
							head.addItem(c);
							c = cCell.createObject(null, {"text": qsTr("res"), "width": 15, "halign": Frame.AlignRight});
							head.addItem(c);
						}
					}
				}
				Detail {
					id: detail
					width: "%"
					layout: Frame.LayoutHorizontal
					//Space { height: 5 }
					Cell {
						width: cellClassName.renderedWidth
						text: detail.data(detail.currentIndex, "classes.name");
					}
					Component.onCompleted: {
						//console.warn("=============", root.stageCount)
						for(var i=0; i<root.stageCount; i++) {
							var fld_run_cnt = "e" + (i+1) + "_runCount";
							var fld_map_cnt = "e" + (i+1) + "_mapCount";
							var c = cBandCell.createObject(null, {"width": 15, "halign": Frame.AlignRight, "fieldName": fld_run_cnt});
							detail.addItem(c);

							c = cBandCell.createObject(null, {"width": 15, "halign": Frame.AlignRight, "fieldName": fld_map_cnt});
							detail.addItem(c);
							c = cMapDiffCell.createObject(null, {"width": 15, "halign": Frame.AlignRight, "stage": (i+1)});
							detail.addItem(c);
						}
					}
				}
				Frame {
					id: footer
					layout: Frame.LayoutHorizontal
					textStyle: myStyle.textStyleBold
					topBorder: Pen { basedOn: "black1" }
					Cell {
						width: cellClassName.renderedWidth
						text: qsTr("Sum")
					}
					Component.onCompleted: {
						//console.debug("stageCount", root.stageCount)
						for(var i=0; i<root.stageCount; i++) {
							var fld_run_cnt = "e" + (i+1) + "_runCount";
							var fld_map_cnt = "e" + (i+1) + "_mapCount";
							var c = cSumCell.createObject(null, {"fieldName": fld_run_cnt, "width": 15, "halign": Frame.AlignRight});
							footer.addItem(c);
							c = cSumCell.createObject(null, {"fieldName": fld_map_cnt, "width": 15, "halign": Frame.AlignRight});
							footer.addItem(c);
							c = cCell.createObject(null, {"text": "", "width": 15, "halign": Frame.AlignRight});
							footer.addItem(c);
						}
					}
				}
			}
		}
	}
}


