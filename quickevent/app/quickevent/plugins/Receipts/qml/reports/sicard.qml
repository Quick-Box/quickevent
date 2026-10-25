import QtQml 2.0
import qf.qmlreports 1.0
import shared.qml.reports 1.0
import "qrc:/quickevent/core/js/timeext.js" as TimeExt
import "qrc:/quickevent/core/js/ogtime.js" as OGTime

Report {
	id: root
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

	width: 210 / 3 - 5
	height: 297
	hinset: 0
	vinset: 5

	function siMSecToString(msec)
	{
		if(msec === 0xeeee * 1000)
			return "--:--:--";
		return TimeExt.msecToString_hhmmss(msec)
	}

	Frame {
		width: "%"
		hinset: 2
		vinset: 2
		border: Pen { basedOn: "blue05" }
		Band {
			id: bandCard
			modelData: "card"
			width: "%"
			Frame {
				htmlExportAttributes: {"lpt_textAlign": "left", "lpt_borderTop": "=", "lpt_borderBottom": "-"}
				width: "%"
				fill: Brush { color: Color { def: "powderblue" } }
				topBorder: Pen { basedOn: "black2" }
				bottomBorder: Pen { basedOn: "black1" }
				hinset: 1
				vinset: 1
				Para {
					textFn: function() {
						var s = "";
						var stage_cnt = bandCard.data("stageCount")
						if(stage_cnt > 1)
							s = qsTr("E") + bandCard.data("currentStageId") + " - ";
						s += bandCard.data("event.name")
						return s;
					}
				}
				Para {
					textFn: function() {
						return TimeExt.dateToISOString(bandCard.data("event.date")) + " " + bandCard.data("event.place")
					}
				}
			}
			Frame {
				fill: brushError
				Para {
					htmlExportAttributes: {"lpt_textStyle": "bold", "lpt_textAlign": "center", "lpt_textWidth": "%"}
					width: "%"
					textStyle: myStyle.textStyleBold
					textHAlign: Frame.AlignHCenter
					topBorder: Pen { basedOn: "black2" }
					//bottomBorder: Pen { basedOn: "black2" }
					text: qsTr("Unassigned card !!!");
				}
				Para {
					htmlExportAttributes: {"lpt_textAlign": "center", "lpt_borderBottom": "=", "lpt_textWidth": "%"}
					width: "%"
					//textStyle: myStyle.textStyleBold
					textHAlign: Frame.AlignHCenter
					//topBorder: Pen { basedOn: "black2" }
					bottomBorder: Pen { basedOn: "black2" }
					text: qsTr("It will not be included in the results.");
				}
			}
			Frame {
				htmlExportAttributes: {"lpt_borderBottom": "-"}
				hinset: 1
				width: "%"
				bottomBorder: Pen { basedOn: "black1" }
				Frame {
					width: "%"
					layout: Frame.LayoutHorizontal
					Para {
						id: paraCheck
						htmlExportAttributes: {"lpt_textWidth": "7", "lpt_textAlign": "left"}
						width: 15
						text: "Check:"
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						textHAlign: Frame.AlignRight
						textFn: function() {
							var msec = bandCard.data("checkTime") * 1000;
							return siMSecToString(msec)
						}
					}
					Cell {
						htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
						width: paraCheck.width
						textHAlign: Frame.AlignRight
						text: "SI:"
					}
					Para {
						width: "%"
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						textHAlign: Frame.AlignRight
						textFn: function() { return "" + bandCard.data("cardNumber"); }
					}
				}
				Frame {
					width: "%"
					layout: Frame.LayoutHorizontal
					Para {
						htmlExportAttributes: {"lpt_textWidth": "7", "lpt_textAlign": "left"}
						width: paraCheck.width
						text: "Start:"
					}
					Para {
						width: "%"
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						textFn: function() {
							var msec = bandCard.data("startTime") * 1000;
							return siMSecToString(msec)
						}
					}
					Cell {
						htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
						width: paraCheck.width
						textHAlign: Frame.AlignRight
						text: "Finish:"
					}
					Para {
						width: "%"
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						textFn: function() {
							var msec = bandCard.data("finishTime") * 1000 + bandCard.data("finishTimeMs");
							return siMSecToString(msec)
						}
					}
				}
			}
			Detail {
				id: dc
				width: "%"
				layout: Frame.LayoutHorizontal
				expandChildFrames: true
				topBorder: (dc.currentIndex < (dc.rowCount - 1))? null: myStyle.penBlack1
				textStyle: (dc.currentIndex < (dc.rowCount - 1))? null: myStyle.textStyleBold;
				htmlExportAttributes: (dc.currentIndex < (dc.rowCount - 2))? ({}): {"lpt_textStyle": "underline2"};
				Cell {
					htmlExportAttributes: {"lpt_textWidth": "4", "lpt_textAlign": "right"}
					width: 8
					textHAlign: Frame.AlignRight
					text: {
						var pos = dc.data(dc.currentIndex, "position");
						if(pos && dc.currentIndex < (dc.rowCount - 1))
							return pos + ".";
						return "";
					}
				}
				Para {
					htmlExportAttributes: {"lpt_textWidth": "5", "lpt_textAlign": "right"}
					width: 8
					//textHAlign: Frame.AlignRight
					text: (dc.currentIndex < (dc.rowCount - 1))? dc.data(dc.currentIndex, "code"): qsTr("FI");
				}
				Para {
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
					width: "40%"
					textHAlign: Frame.AlignRight
					text: TimeExt.msecToString_hhmmss(dc.data(dc.currentIndex, "punchTimeMs"));
				}
				Para {
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
					width: "%"
					textHAlign: Frame.AlignRight
					text: OGTime.msecToString_mmss(dc.data(dc.currentIndex, "stpTimeMs"), OGTime.TimeMeasurementPrecision.Second);
				}
				Para {
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
					width: "%"
					textHAlign: Frame.AlignRight
					text: OGTime.msecToString_mmss(dc.data(dc.currentIndex, "lapTimeMs"), OGTime.TimeMeasurementPrecision.Second);
				}
			}
			Frame {
				width: "%"
				topBorder: Pen { basedOn: "black2" }
			}
			Frame {
				width: "%"
				hinset: 1
				layout: Frame.LayoutHorizontal
				Para {
					textFn: function() {
						var data = bandCard.data("data");
						var battery_status = data.batteryStatus;
						if(battery_status)
							return qsTr("Siac battery: ") + battery_status.voltage.toFixed(2) + "V - " + (battery_status.low? "LOW": "Ok");
						return "";
					}
				}
			}
		}
	}
}


