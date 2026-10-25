// General relay award diploma template (Typst).
// Data is injected as data.json: one object per diploma with the keys produced by
// AwardTypstRenderer::collectPages (eventName, date, place, positionCategory,
// clubName, runners, mainReferee, director, ...). "runners" is a newline-separated
// list of competitor names.
#set page(width: 210mm, height: 297mm, margin: (x: 15mm, top: 20mm, bottom: 20mm))
#set text(font: "Arial")

#let pages = json("data.json")
#let ml(s) = s.split("\n").join(linebreak())

#for (i, page) in pages.enumerate() [
	#set align(center)

	#text(size: 20pt, weight: "bold")[#page.at("eventName", default: "")]
	#v(2mm)
	#text(size: 11pt, fill: rgb("#cc0000"))[#page.at("date", default: "")]

	#v(12mm)
	#text(font: "Times New Roman", size: 60pt, fill: rgb("#800000"))[Diplom]

	#v(14mm)
	#text(size: 18pt, weight: "bold")[#page.at("clubName", default: "")]
	#v(2mm)
	#text(size: 14pt)[#page.at("positionCategory", default: "")]
	#v(4mm)
	#text(size: 13pt)[#ml(page.at("runners", default: ""))]

	#v(1fr)
	#grid(columns: (1fr, 1fr), gutter: 20mm,
		[
			#text(weight: "bold")[#page.at("mainReferee", default: "")]
			#line(length: 100%)
			Hlavní rozhodčí
		],
		[
			#text(weight: "bold")[#page.at("director", default: "")]
			#line(length: 100%)
			Ředitel závodu
		]
	)

	#if i < pages.len() - 1 [#pagebreak()]
]
