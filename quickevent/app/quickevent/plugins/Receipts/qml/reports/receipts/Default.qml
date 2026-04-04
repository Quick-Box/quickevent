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
	Para {
		omitEmptyText: true
		textFn: function() {
			relayInfoFrame.visible = !!bandCard.data("isRelay");
			return ""
		}
	}
	Frame {
		width: "%"
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
					htmlExportAttributes: {"lpt_textAlign": "center", "lpt_textWidth": "%"}
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
					htmlExportAttributes: {"lpt_textStyle": "bold", "lpt_borderBottom": "-"}
					hinset: 2
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
								return (t)? t: qsTr("NO_REG");
							}
						}
					}
					Frame {
						width: "%"
						layout: Frame.LayoutHorizontal
						Para {
							htmlExportAttributes: {"lpt_textWidth": "%"}
							width: "%"
							//textHAlign: Frame.AlignLeft
							textFn: function() {
								var class_name = detailCompetitor.data(detailCompetitor.currentIndex, "classes.name");
								return class_name ? class_name : "";
							}
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
				width: "%"
				hinset: 2
				vinset: 1
				bottomBorder: Pen { basedOn: "black1" }
				htmlExportAttributes: {"lpt_borderBottom": "=", "lpt_textWidth": "%"}
				Frame {
					id: relayInfoFrame
					width: "%"
					layout: Frame.LayoutHorizontal
					Para {
						width: 12
						htmlExportAttributes: {"lpt_textWidth": "7", "lpt_textAlign": "left"}
						text: qsTr("Relay:")
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						width: 14
						textHAlign: Frame.AlignLeft
						textFn: function() { return bandCard.data("relayNumber"); }
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "%"}
						width: "%"
					}
					Cell {
						width: 12
						htmlExportAttributes: {"lpt_textWidth": "7", "lpt_textAlign": "left"}
						text: qsTr("Leg:")
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						width:14
						textHAlign: Frame.AlignLeft
						textFn: function() { return bandCard.data("leg"); }
					}
				}
				Frame {
					width: "%"
					layout: Frame.LayoutHorizontal
					Para {
						id: paraCheck
						width: 12
						htmlExportAttributes: {"lpt_textWidth": "7", "lpt_textAlign": "left"}
						text: qsTr("Check:")
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						id: paraCheckTime
						width: 14
						textHAlign: Frame.AlignLeft
						textFn: function() {
							var start00msec = bandCard.data("stageStartTimeMs");
							return TimeExt.msecToString_hhmmss(start00msec + bandCard.data("checkTimeMs"));
						}
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "%"}
						width: "%"
					}
					Cell {
						width: paraCheck.width
						htmlExportAttributes: {"lpt_textWidth": "7", "lpt_textAlign": "left"}
						text: qsTr("SI:")
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						width: paraCheckTime.width
						textHAlign: Frame.AlignLeft
						textFn: function() { return bandCard.data("cardNumber"); }
					}
				}
				Frame {
					width: "%"
					layout: Frame.LayoutHorizontal
					Para {
						htmlExportAttributes: {"lpt_textWidth": "7", "lpt_textAlign": "left"}
						width: paraCheck.width
						text: qsTr("Start:")
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						width: paraCheckTime.width
						textHAlign: Frame.AlignLeft
						textFn: function() {
							var start00msec = bandCard.data("stageStartTimeMs");
							return TimeExt.msecToString_hhmmss(start00msec + bandCard.data("startTimeMs"));
						}
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "%"}
						width: "%"
					}
					Cell {
						width: paraCheck.width
						htmlExportAttributes: {"lpt_textWidth": "7", "lpt_textAlign": "left"}
						text: qsTr("Finish:")
					}
					Para {
						htmlExportAttributes: {"lpt_textWidth": "9", "lpt_textAlign": "right"}
						width: paraCheckTime.width
						textHAlign: Frame.AlignLeft
						textFn: function() {
							var start00msec = bandCard.data("stageStartTimeMs");
							return TimeExt.msecToString_hhmmss(start00msec + bandCard.data("finishTimeMs"));
						}
					}
				}
			}
			Detail {
				id: dc
				width: "%"
				hinset: 2
				layout: Frame.LayoutHorizontal
				expandChildFrames: true
				fill: {
					var tm = data(dc.currentIndex, "stpTimeMs");
					if(!tm)
						return brushError;
					return brushNone;
				}
				topBorder: (dc.currentIndex < (dc.rowCount - 1))? null: myStyle.penBlack1

				Frame {
					// fake para, just to run code and set visibility of cardLentFrame
					Para {
						omitEmptyText: true
						textFn: function() {
							var is_last_row = (dc.currentIndex == (dc.rowCount - 1));
							controlCodeFrame.visible = !is_last_row;
							cellRunStatus.visible = is_last_row;
							return "";
						}
					}
				}

				Frame {
					id: controlCodeFrame
					layout: Frame.LayoutHorizontal
					Para {
						id: cellPos
						htmlExportAttributes: {"lpt_textWidth": "2", "lpt_textAlign": "right"}
						width: 4
						textHAlign: Frame.AlignRight
						text: {
							var pos = dc.data(dc.currentIndex, "position");
							if(pos && dc.currentIndex < (dc.rowCount - 1))
								return pos;
							return "";
						}
					}
					Para {
						id: cellCodeOpen
						htmlExportAttributes: {"lpt_textWidth": "2"}
						width: 2
						text: " ("
					}
					Para {
						id: cellCode
						htmlExportAttributes: {"lpt_textWidth": "3", "lpt_textAlign": "center"}
						width: 6
						textHAlign: Frame.AlignHCenter
						text: { return dc.data(dc.currentIndex, "code"); }
					}
					Para {
						id: cellCodeClose
						htmlExportAttributes: {"lpt_textWidth": "1"}
						width: 1
						text: ")"
					}
				}
				Frame {
					id: cellRunStatus
					width:  10
					vinset: 1
					halign: Frame.AlignHCenter
					Para {
						htmlExportAttributes: {"lpt_textWidth": "8", "lpt_textAlign": "center", "lpt_textStyle": "bold"}
						textFn: function() { return bandCard.data("isOk")? qsTr("OK"): qsTr("DISQ"); }
						textStyle: myStyle.textStyleBold;
					}
				}



				Para {
					htmlExportAttributes: {"lpt_textWidth": "%"}
					width: "%"
				}
				Para {
					id: cellStp
					htmlExportAttributes: (dc.currentIndex < (dc.rowCount - 1) && dc.data(dc.currentIndex, "standCummulative") !== 1)? {"lpt_textWidth": "7", "lpt_textAlign": "right"} : {"lpt_textWidth": "7", "lpt_textAlign": "right", "lpt_textStyle": "bold"}
					width: (dc.currentIndex < (dc.rowCount - 1))? 12 : 14;
					textHAlign: Frame.AlignRight
					text: {
						var msec = dc.data(dc.currentIndex, "stpTimeMs");
						if(msec > 0)
							return " " + OGTime.msecToString_mmss(msec);
						return qsTr("-----");
					}
					textStyle: (dc.currentIndex < (dc.rowCount - 1) && dc.data(dc.currentIndex, "standCummulative") !== 1)? null: myStyle.textStyleBold;
					vinset: (dc.currentIndex < (dc.rowCount - 1)) ? 0 : 1
				}

				Para {
					id: cellStandOpen
					htmlExportAttributes: (dc.currentIndex < (dc.rowCount - 1) && dc.data(dc.currentIndex, "standCummulative") !== 1)? {"lpt_textWidth": "2"} : {"lpt_textWidth": "2", "lpt_textStyle": "bold"}
					width: 2
					textFn: function() {
						var is_last_row = dc.currentIndex == (dc.rowCount - 1);
						var msec = dc.data(dc.currentIndex, "lapTimeMs")
						if (msec > 0 && (!is_last_row || bandCard.data("isOk")))
							return " (";
						return  "";
					}
					textStyle: (dc.currentIndex < (dc.rowCount - 1) && dc.data(dc.currentIndex, "standCummulative") !== 1)? null: myStyle.textStyleBold;
					vinset: (dc.currentIndex < (dc.rowCount - 1)) ? 0 : 1
				}
				Para {
					id: cellStand
					width: (dc.currentIndex < (dc.rowCount - 1))? 4 : 5;
					htmlExportAttributes: (dc.currentIndex < (dc.rowCount - 1) && dc.data(dc.currentIndex, "standCummulative") !== 1)? {"lpt_textWidth": "2", "lpt_textAlign": "center"} : {"lpt_textWidth": "2", "lpt_textAlign": "center", "lpt_textStyle": "bold"}
					textHAlign: Frame.AlignHCenter
					textFn: function() {
						var msec = dc.data(dc.currentIndex, "lapTimeMs");
						var is_last_row = dc.currentIndex == (dc.rowCount - 1);
						var lap_stand = dc.data(dc.currentIndex, "standCummulative");
						if (msec  > 0 && (!is_last_row || bandCard.data("isOk")))
							return lap_stand;
						return "";
					}
					textStyle: (dc.currentIndex < (dc.rowCount - 1) && dc.data(dc.currentIndex, "standCummulative") !== 1)? null: myStyle.textStyleBold;
					vinset: (dc.currentIndex < (dc.rowCount - 1)) ? 0 : 1
				}
				Para {
					id: cellStandClose
					htmlExportAttributes: (dc.currentIndex < (dc.rowCount - 1) && dc.data(dc.currentIndex, "standCummulative") !== 1)? {"lpt_textWidth": "1"} : {"lpt_textWidth": "1", "lpt_textStyle": "bold"}
					width: 1
					textFn: function() {
						var is_last_row = dc.currentIndex == (dc.rowCount - 1);
						var msec = dc.data(dc.currentIndex, "lapTimeMs")
						if (msec > 0 && (!is_last_row || bandCard.data("isOk")))
							return ")";
						return  "";
					}
					textStyle: (dc.currentIndex < (dc.rowCount - 1) && dc.data(dc.currentIndex, "standCummulative") !== 1)? null: myStyle.textStyleBold;
					vinset: (dc.currentIndex < (dc.rowCount - 1)) ? 0 : 1
				}

				Para {
					htmlExportAttributes: {"lpt_textWidth": "%"}
					width: "%"
				}
				Para {
					id: cellLap
					htmlExportAttributes: (dc.data(dc.currentIndex, "standLap") !== 1)? {"lpt_textWidth": "6", "lpt_textAlign": "right"} : {"lpt_textWidth": "6", "lpt_textAlign": "right", "lpt_textStyle": "bold"}
					width: 10
					textHAlign: Frame.AlignRight
					textFn: function() {
						var msec = dc.data(dc.currentIndex, "lapTimeMs");
						if(msec > 0)
							return OGTime.msecToString_mmss(msec);
						return qsTr("-----");
					}
					textStyle: (dc.data(dc.currentIndex, "standLap") !== 1)? null: myStyle.textStyleBold;
				}
				Para {
					id: cellLapStandOpen
					htmlExportAttributes: (dc.data(dc.currentIndex, "standLap") !== 1)? {"lpt_textWidth": "2"} : {"lpt_textWidth": "2", "lpt_textStyle": "bold"}
					width: 2
					text: (dc.data(dc.currentIndex, "lapTimeMs") > 0) ? " (" : "";
					textStyle: (dc.data(dc.currentIndex, "standLap") !== 1)? null: myStyle.textStyleBold;
				}
				Para {
					id: cellLapStand
					width: 4
					htmlExportAttributes: (dc.data(dc.currentIndex, "standLap") !== 1)? {"lpt_textWidth": "2", "lpt_textAlign": "center"} : {"lpt_textWidth": "2", "lpt_textAlign": "center", "lpt_textStyle": "bold"}
					textHAlign: Frame.AlignHCenter
					textFn: function() {
						var msec = dc.data(dc.currentIndex, "lapTimeMs");
						var lap_stand = dc.data(dc.currentIndex, "standLap");
						if(msec > 0)
							return lap_stand;
						return "";
					}
					textStyle: (dc.data(dc.currentIndex, "standLap") !== 1)? null: myStyle.textStyleBold;
				}
				Para {
					id: cellLapStandClose
					htmlExportAttributes: (dc.data(dc.currentIndex, "standLap") !== 1)? {"lpt_textWidth": "1"} : {"lpt_textWidth": "1", "lpt_textStyle": "bold"}
					width: 1
					text: (dc.data(dc.currentIndex, "lapTimeMs") > 0) ? ")" : "";
					textStyle: (dc.data(dc.currentIndex, "standLap") !== 1)? null: myStyle.textStyleBold;
				}

				Para {
					width: "%"
					htmlExportAttributes: {"lpt_textWidth": "%"}
				}
				Para {
					width: 2
					htmlExportAttributes: {"lpt_textWidth": "2"}
					text: {
						var msec = dc.data(dc.currentIndex, "lossMs");
						if(msec > 0)
							return " +";
						return "";
					}
				}
				Para {
					htmlExportAttributes: {"lpt_textWidth": "5", "lpt_textAlign": "right"}
					id: cellLoss
					width: 9
					textHAlign: Frame.AlignRight
					text: {
						var msec = dc.data(dc.currentIndex, "lossMs");
						if(msec > 0)
							return OGTime.msecToString_mmss(msec);
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
						var is_ok = bandCard.data("isOk");
						performanceInfoFrame.visible = !!is_ok;
						return "";
					}
				}
			}
			Frame {
				id: cardLentFrame
				hinset: 2
				htmlExportAttributes: {"lpt_borderBottom": "="}
				fill: brushError
				bottomBorder: Pen { basedOn: "black1" }
				textStyle: myStyle.textStyleBold
				Para {
					htmlExportAttributes: {"lpt_textWidth": "%", "lpt_textAlign": "center", "lpt_textStyle": "bold"}
					width: "%"
					textHAlign: Frame.AlignHCenter
					text: qsTr("!!! RENTED CARD !!!");
				}
			}

			Frame {
				id: performanceInfoFrame
				htmlExportAttributes: {"lpt_textWidth": "%"}
				width: "%"
				vinset: 1
				hinset: 2
				layout: Frame.LayoutVertical
				Para {
					textFn: function() {
						var current_standings = bandCard.data("currentStandings");
						var competitors_count = bandCard.data("competitorsFinished");
						if(current_standings && competitors_count)
							return qsTr("current placement = ") + current_standings + ". / " + competitors_count;
						return "";
					}
				}
				Para {
					textFn: function() {
						var dm = bandCard.dataModel;
						var loss = bandCard.data("timeMs") - bandCard.data("bestTime");
						return qsTr("loss to leading runner = ") + OGTime.msecToString_mmss(loss);
					}
				}
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
						return qsTr("loss to best splits =  ") + OGTime.msecToString_mmss(overall_loss);
					}
				}
				Para {
					textFn: function() {
						var time = bandCard.data("timeMs");
						var length = root.courseLength;
						if(length > 0)
							return qsTr("average pace = ") + OGTime.msecToString_mmss(((time / length) >> 0) * 1000) + " min/km";
						return "";
					}
				}
			}
			Para {
				hinset: 2
				textFn: function() {
					var extra_codes = bandCard.data("extraCodes");
					//console.warn("missing_codes:", JSON.stringify(missing_codes, null, 2));
					if(extra_codes.length > 0) {
						var xcs = [];
						for(var i=0; i<extra_codes.length; i++) {
							var xca = extra_codes[i];
							xcs.push(xca[0] + "-" + xca[1]);
						}
						return qsTr("extra punches = ") + xcs.join(", ");
					}
					return "";
				}
			}
			Frame {
				id: receiptQrCodeFrame
				width: "%"
				halign: Frame.AlignHCenter
				hinset: 2
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


