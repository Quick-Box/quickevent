import QtQml 2.0
import qf.core 1.0
import qf.qmlreports 1.0
import shared.qml.reports 1.0
import "qrc:/quickevent/core/js/timeext.js" as TimeExt
import "qrc:/quickevent/core/js/ogtime.js" as OGTime
import "private" as Private

Report {
	id: root
	property int courseLength: 0
	property bool printLotteryTicket: false
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

	Frame {
		width: "%"
		hinset: 2
		vinset: 2
		border: Pen { basedOn: "blue05" }
		Band {
			id: bandCompetitor
			objectName: "band_competitor"
			modelData: "competitor"
			width: "%"
			Detail {
				id: detailCompetitor
				width: "%"
				layout: Frame.LayoutVertical
				bottomBorder: Pen { basedOn: "black2" }
				Private.LotteryTicket {
					visible: printLotteryTicket
				}
				Frame {
					htmlExportAttributes: {"lpt_textAlign": "left", "lpt_borderTop": "=", "lpt_borderBottom": "-"}
					id: receiptImageFrame
					width: "%"
					halign: Frame.AlignHCenter
					hinset: 4
					vinset: 1
					Para {
						omitEmptyText: true
						textFn: function() {
							var path = bandCompetitor.data("event.receiptImagePath") || "";
							var image_height = bandCompetitor.data("event.receiptImageHeightMm") || 18;
							if(image_height < 10)
								image_height = 10;
							else if(image_height > 60)
								image_height = 60;
							receiptImage.visible = !!path;
							receiptImage.height = path ? image_height : 0;
							receiptImage.dataSource = path;
							return "";
						}
					}
					Image {
						id: receiptImage
						visible: false
						height: 0
						dataSource: ""
					}
				}
				Frame {
					htmlExportAttributes: {"lpt_textStyle": "bold"}
					hinset: 1
					vinset: 1
					textStyle: myStyle.textStyleBold
					fill: Brush { color: Color { def: "khaki" } }
					Frame {
						width: "%"
						layout: Frame.LayoutHorizontal
						Para {
							width: "%"
							textFn: function() {
								var name = detailCompetitor.data(detailCompetitor.currentIndex, "competitorName");
								return name ? name : "";
							}
						}
						Para {
							htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
							textHAlign: Frame.AlignRight
							text: {
								var t = detailCompetitor.data(detailCompetitor.currentIndex, "competitors.registration");
								return (t)? t: "NO_REG";
							}
						}
					}
					Frame {
						htmlExportAttributes: {"lpt_textStyle": "underline2"}
						width: "%"
						layout: Frame.LayoutHorizontal
						Para {
							htmlExportAttributes: {"lpt_textWidth": "%"}
							width: "%"
							//textHAlign: Frame.AlignLeft
							textFn: function() {
								var class_name = detailCompetitor.data(detailCompetitor.currentIndex, "classes.name");
								return class_name ? class_name : "";
							} //"SI: " + detailCompetitor.data(detailCompetitor.currentIndex, "runs.siId")
						}
						Para {
							textFn: function() {
								var l = bandCompetitor.data("courses.length");
								root.courseLength = (l)? l : 0;
								return (root.courseLength / 1000) + "km";
							}
						}
						Para {
							textFn: function() {
								return " " + bandCompetitor.data("courses.climb") + "m";
							}
						}
					}
				}
			}
		}
		Band {
			id: bandCard
			objectName: "band_card"
			modelData: "card"
			width: "%"
			Frame {
				hinset: 1
				width: "%"
				bottomBorder: Pen { basedOn: "black1" }
				htmlExportAttributes: {"lpt_borderBottom": "="}
				Frame {
					layout: Frame.LayoutHorizontal
					Para {
						id: paraCheck
						width: 11
						text: "Check:"
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						id: paraCheckTime
						width: 15
						textHAlign: Frame.AlignRight
						textFn: function() {
							var start00msec = bandCard.data("stageStartTimeMs");
							return TimeExt.msecToString_hhmmss(start00msec + bandCard.data("checkTimeMs"));
						}
					}
					Cell {
						htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
						width: "%"
						textHAlign: Frame.AlignRight
						text: "Finish:"
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						width: paraCheckTime.width
						textHAlign: Frame.AlignRight
						textFn: function() {
							var start00msec = bandCard.data("stageStartTimeMs");
							return TimeExt.msecToString_hhmmss(start00msec + bandCard.data("finishTimeMs"));
						}
					}
				}
				Frame {
					htmlExportAttributes: {"lpt_textStyle": "underline2"}
					width: "%"
					layout: Frame.LayoutHorizontal
					Para {
						width: paraCheck.width
						text: "Start:"
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						width: paraCheckTime.width
						textHAlign: Frame.AlignRight
						textFn: function() {
							var start00msec = bandCard.data("stageStartTimeMs");
							return TimeExt.msecToString_hhmmss(start00msec + bandCard.data("startTimeMs"));
						}
					}
					Cell {
						text: "/"
					}
					Para {
						textFn: function() {
							var start = bandCard.data("startTimeMs");
							return OGTime.msecToString_mmss(start)
						}
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
						width: "%"
						textHAlign: Frame.AlignRight
						textFn: function() { return "SI:" + bandCard.data("cardNumber"); }
					}
				}
			}
			Detail {
				id: dc
				width: "%"
				layout: Frame.LayoutHorizontal
				expandChildFrames: true
				fill: {
					var tm = data(dc.currentIndex, "stpTimeMs");
					if(!tm)
						return brushError;
					return brushNone;
				}
				topBorder: (dc.currentIndex < (dc.rowCount - 1))? null: myStyle.penBlack1
				textStyle: (dc.currentIndex < (dc.rowCount - 1))? null: myStyle.textStyleBold;
				htmlExportAttributes: (dc.currentIndex < (dc.rowCount - 2))? ({}): {"lpt_textStyle": "underline2"};
				Cell {
					id: cellPos
					htmlExportAttributes: {"lpt_textWidth": "3", "lpt_textAlign": "right"}
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
					id: cellCode
					htmlExportAttributes: {"lpt_textWidth": "4", "lpt_textAlign": "right"}
					width: 7
					//textHAlign: Frame.AlignRight
					textFn: function() {
						var ret;
						if(dc.currentIndex < (dc.rowCount - 1)) {
							ret = dc.data(dc.currentIndex, "code");
						}
						else {
							ret = bandCard.data("isOk")? qsTr("OK"): qsTr("DISQ");
						}
						return ret;
					}
				}
				Para {
					id: cellStp
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
					width: "%"
					textHAlign: Frame.AlignRight
					text: {
						var msec = dc.data(dc.currentIndex, "stpTimeMs");
						if(msec > 0)
							return OGTime.msecToString_mmss(msec);
						return qsTr("-----");
					}
				}
				Para {
					id: cellLap
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
					width: "%"
					textHAlign: Frame.AlignRight
					text: {
						var msec = dc.data(dc.currentIndex, "lapTimeMs");
						if(msec > 0)
							return OGTime.msecToString_mmss(msec);
						return qsTr("-----");
					}
				}
				Para {
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
					id: cellLoss
					width: "%"
					textHAlign: Frame.AlignRight
					text: {
						var msec = dc.data(dc.currentIndex, "lossMs");
						if(msec > 0)
							return "+" + OGTime.msecToString_mmss(msec);
						return "";
					}
				}
				Para {
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
					id: cellMinKm
					width: "%"
					textHAlign: Frame.AlignRight
					text: {
						var msec = dc.data(dc.currentIndex, "lapTimeMs");
						var km = dc.data(dc.currentIndex, "distance") / 1000;
						if(msec > 0 && km > 0) {
							return "~" + OGTime.msecToString_mmss(msec / km);
						}
						return "";
					}
				}
			}
			Frame {
				width: "%"
				bottomBorder: Pen { basedOn: "black2" }
				htmlExportAttributes: {"lpt_borderTop": "="}
				// fake para, just to run code and set visibility of cardLentFrame
				Para {
					omitEmptyText: true
					textFn: function() {
						var card_lent = bandCard.data("isCardLent");
						cardLentFrame.visible = !!card_lent
						return "";
					}
				}
			}
			Frame {
				id: cardLentFrame
				//visible: false
				//vinset: 1
				htmlExportAttributes: {"lpt_borderBottom": "="}
				fill: brushError
				bottomBorder: Pen { basedOn: "black1" }
				textStyle: myStyle.textStyleBold
				Para {
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "center"}
					width: "%"
					textHAlign: Frame.AlignHCenter
					text: qsTr("!!! RENTED CARD !!!");
				}
			}
			Para {
				//vinset: 1
				hinset: 1
				textFn: function() {
					var extra_codes = bandCard.data("extraCodes");
					//console.warn("missing_codes:", JSON.stringify(missing_codes, null, 2));
					if(extra_codes) {
						var xcs = [];
						for(var i=0; i<extra_codes.length; i++) {
							var xca = extra_codes[i];
							xcs.push(xca[0] + "-" + xca[1]);
						}
						var ret = qsTr("extra: ") + xcs.join(", ");
						return ret;
					}
					return "";
				}
			}
			Para {
				//vinset: 1
				hinset: 1
				textFn: function() {
					var bad_check = bandCard.data("isBadCheck");
					if(bad_check) {
						return qsTr("BAD CHECK !!!");
					}
					return "";
				}
			}
			Frame {
				width: "%"
				//vinset: 1
				hinset: 1
				layout: Frame.LayoutHorizontal
				Para {
					textFn: function() {
						var dm = bandCard.dataModel;
						var overall_loss = 0;
						//console.info("AHOJ", dm.dump());
						for(var i = 0; i < dm.rowCount(); i++) {
							var loss = dm.dataByName(i, 'lossMs');
							//console.info(i, loss);
							overall_loss += loss;
						}
						return qsTr("current overall loss: +") + OGTime.msecToString_mmss(overall_loss);
					}
				}
			}
			Frame {
				width: "%"
				//vinset: 1
				hinset: 1
				layout: Frame.LayoutHorizontal
				//htmlExportAttributes: {"lpt_borderTop": "-"}
				Para {
					textFn: function() {
						var current_standings = bandCard.data("currentStandings");
						var competitors_count = bandCard.data("competitorsFinished");
						if(current_standings && competitors_count)
							return qsTr("standings: ") + current_standings + "/" + competitors_count;
						return "";
					}
				}
				Para {
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "right"}
					width: "%"
					textHAlign: Frame.AlignRight
					textFn: function() {
						var time = bandCard.data("timeMs");
						var length = root.courseLength;
						if(length > 0)
							return OGTime.msecToString_mmss(((time / length) >> 0) * 1000) + "min/km";
						return "";
					}
				}
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
			Frame {
				id: receiptQrCodeFrame
				width: "%"
				halign: Frame.AlignHCenter
				hinset: 1
				vinset: 2
				Para {
					omitEmptyText: true
					textFn: function() {
						var path = bandCard.data("event.receiptQrCodePath") || "";
						receiptQrCodeImage.visible = !!path;
						receiptQrCodeImage.width = path ? 24 : 0;
						receiptQrCodeImage.height = path ? 24 : 0;
						receiptQrCodeImage.dataSource = path;
						return "";
					}
				}
				Image {
					id: receiptQrCodeImage
					visible: false
					width: 0
					height: 0
					dataSource: ""
				}
				Para {
					omitEmptyText: true
					width: "%"
					htmlExportAttributes: {"lpt_textAlign": "center", "lpt_textWidth": "%"}
					textHAlign: Frame.AlignHCenter
					textFn: function() {
						var path = bandCard.data("event.receiptQrCodePath") || "";
						if(!path)
							return "";
						return bandCard.data("event.receiptQrCodeCaption") || "";
					}
				}
			}
		}
	}
}


