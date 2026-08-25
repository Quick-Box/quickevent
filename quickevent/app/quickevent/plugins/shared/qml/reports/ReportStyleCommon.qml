import QtQml 2.0
import qf.core 1.0
import qf.qmlreports 1.0

StyleSheet {
	property alias textStyleDefault: tsDefault
	property alias textStyleBig: tsBig
	property alias textStyleBold: tsBold
	property alias textStyleStrikeOut: tsStrikeOut

	property alias penBlack1: penBlack1
	property alias penBlue1: penBlack1
	objectName: "MyStyleSheet"
	//basedOn: "by_name"
	colors: [
		Color {name: "none"; def: "#00000000"},
		Color {name: "pink"; def: "#fd8184"},
		Color {name: "peach"; def: "lightgoldenrodyellow"},
		Color {id: colorKhaki; name: "khaki"; def: "khaki"},
		Color {id: colorSalmon; name: "salmon"; def: "salmon"}
	]
	pens: [
		Pen {
			name: "black05"
			color: Color {def:Qt.black}
			width: 0.5
		},
		Pen {
			id: penBlack1
			name: "black1"
			color: Color {def:Qt.black}
			width: 1
		},
		Pen {
			name: "black2"
			basedOn: "black1";
			width: 2
		},
		Pen {
			name: "black1dot";
			basedOn: "black1";
			style: Pen.DotLine
		},
		Pen {
			name: "black1dash";
			basedOn: "black1";
			style: Pen.DashLine
		},
		Pen {
			name: "blue05"
			basedOn: "black05"
			color: Color {def:"blue"}
		},
		Pen {
			id: penBlue1
			name: "blue1"
			basedOn: "black1"
			color: Color {def:"blue"}
		},
		Pen {
			name: "blue1dash"
			basedOn: "black1dash"
			color: Color {def:"blue"}
		}
	]
	brushes: [
		Brush {
			name: "khaki"
			color: colorKhaki
		},
		Brush {
			name: "salmon"
			color: colorSalmon
		}
	]
	fonts: [
		Font {
			id: fontDefault
			name: "default"
			pointSize: 9
		},
		Font {
			id: fontBold
			name: "bold"
			basedOn: "default"
			weight: Font.WeightBold
		},
		Font {
			id: fontBoldItalic
			basedOn: "bold"
			style: Font.StyleItalic
		},
		Font {
			id: fontItalic
			basedOn: "default"
			style: Font.StyleItalic
		},
		Font {
			id: fontStrikeOut
			name: "strikeOut"
			basedOn: "default"
			strikeOut: Font.StrikeOutTrue
		},
		Font {
			id: fontBig
			pointSize: fontDefault.pointSize * 2
		}
	]
	textStyles: [
		TextStyle {
			id: tsDefault
			name: "default"
			font: fontDefault
		},
		TextStyle {
			id: tsBold
			name: "bold"
			font: fontBold
		},
		TextStyle {
			name: "tableHeader"
			font: fontBoldItalic
		},
		TextStyle {
			name: "reportHeader"
			font: fontItalic
			pen: penBlue1
		},
		TextStyle {
			id: tsStrikeOut
			name: "strikeOut"
			font: fontStrikeOut
		},
		TextStyle {
			id: tsBig
			name: "big"
			font: fontBig;
		}
	]
}
