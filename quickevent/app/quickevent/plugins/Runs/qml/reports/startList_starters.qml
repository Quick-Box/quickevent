import qf.qmlreports 1.0
import shared.qml.reports 1.0
import "qrc:/quickevent/core/js/ogtime.js" as OGTime

Report {
	id: root
	objectName: "root"

	property string reportTitle: qsTr("Start list for starters")
	property var options
	property var eventConfig
	property var stageConfig
	property int stageId
	property bool isPrintStartNumbers: options.isStartListPrintStartNumbers? true: false
	property int lineSpacing: options.startersOptionsLineSpacing > 0? options.startersOptionsLineSpacing: 0

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
			columns: {
				//console.info(JSON.stringify(root.options));
				root.options.columns
			}
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
					//Space { height: 5 }
					Frame {
						id: f1
						objectName: "minuteHeaderFrame"
						// hide recent value of start time in object to avoid property binding loop
						// property int prevStartTimeMin: -1 // property binding loop
						property var prev: ({startTimeMin: -1});
						width: "%"
						layout: Frame.LayoutHorizontal
						fill: Brush {color: Color {def: "khaki"} }
						Cell {
							objectName: "minuteHeaderCell"
							width: "10"
							text: {
								var t = detail.data(detail.currentIndex, "startTimeMin");
								//console.warn(f1.prev, f1.prev.startTimeMin, t)
								if(t !== f1.prev.startTimeMin) {
									f1.prev.startTimeMin = t;
									return t;
								}
								return "";
							}
							textStyle: myStyle.textStyleBold
						}
						Cell {
							objectName: "minuteHeaderCell2"
							width: "%"
							text: {
								var t = detail.data(detail.currentIndex, "startTimeText");
								if(t !== f1.prev.startTimeText) {
									f1.prev.startTimeText = t;
									return t;
								}
								return "";
							}
							textStyle: myStyle.textStyleBold
						}
					}
					Frame {
						objectName: "minuteFrame"
						width: "%"
						layout: Frame.LayoutHorizontal
						Para {
							objectName: "minuteCellClassName"
							width: 10
							text: detail.data(detail.currentIndex, "classes.name");
						}
						Para {
							visible: root.isPrintStartNumbers
							width: 8
							halign: Frame.AlignRight
							text: {
								var sn = detail.data(detail.currentIndex,"competitors.startNumber");
								return sn > 0? sn: "";
							}
						}
						Cell {
							objectName: "minuteCellCompetitorName"
							width: "%"
							text: detail.data(detail.currentIndex, "competitorName");
						}
						Para {
							objectName: "minuteCellRegistration"
							width: 16
							text: detail.data(detail.currentIndex, "competitors.registration");
						}
						Para {
							objectName: "minuteCellSI"
							width: 15
							halign: Frame.AlignRight
							text: detail.data(detail.currentIndex, "runs.siId");
						}
						Para {
							objectName: "minuteCellStartTimeMs"
							width: 12
							halign: Frame.AlignRight
							text: OGTime.msecToString_mmss(detail.data(detail.currentIndex, "startTimeMs"), OGTime.TimeMeasurementPrecision.Second);
						}
					}
					// strihani pro Zbyndu
					Frame {
						visible: root.lineSpacing > 0;
						height: root.lineSpacing;
					}
				}
			}
		}
	}
}


