// @design pageW=297.000 pageH=210.000 bgPath=awards_bg_ceskyorientak_maskot.png
#set page(width: 297.000mm, height: 210.000mm, margin: 0mm, background: image("awards_bg_ceskyorientak_maskot.png", width: 100%, height: 100%, fit: "cover"))
#let pages = json("data.json")
#for (i, page) in pages.enumerate() [
  // @item kind=1 x=7.000 y=179.500 w=113.200 h=20.000 zOrder=0 imagePath= text=%24%28eventName%29%0A%24%28place%29%20%24%28date%29 fontFamily=Space%20Grotesk fontSize=20 bold=1 italic=0 color=%23f37021 halign=4 scaleProportional=1
  #let val0 = str(page.at("eventName", default: "")) + "\n" + str(page.at("place", default: "")) + " " + str(page.at("date", default: ""))
  #if val0 != "" [
    #place(dx: 7.000mm, dy: 179.500mm, box(width: 113.200mm, height: 20.000mm, align(center + horizon, text(font: "Space Grotesk", size: 20pt, weight: "bold", style: "normal", fill: rgb("#f37021"))[#(val0.split("\n").join(linebreak()))]))) 
  ]
  // @item kind=1 x=185.000 y=117.500 w=60.000 h=10.000 zOrder=0 imagePath= text=%24%28category%29 fontFamily=Space%20Grotesk fontSize=20 bold=1 italic=0 color=%23f37021 halign=4 scaleProportional=1
  #let val1 = str(page.at("category", default: ""))
  #if val1 != "" [
    #place(dx: 185.000mm, dy: 117.500mm, box(width: 60.000mm, height: 10.000mm, align(center + horizon, text(font: "Space Grotesk", size: 20pt, weight: "bold", style: "normal", fill: rgb("#f37021"))[#(val1.split("\n").join(linebreak()))]))) 
  ]
  // @item kind=1 x=56.800 y=106.500 w=8.800 h=10.000 zOrder=0 imagePath= text=%24%28position%29 fontFamily=Space%20Grotesk fontSize=20 bold=1 italic=0 color=%23f37021 halign=4 scaleProportional=1
  #let val2 = str(page.at("position", default: ""))
  #if val2 != "" [
    #place(dx: 56.800mm, dy: 106.500mm, box(width: 8.800mm, height: 10.000mm, align(center + horizon, text(font: "Space Grotesk", size: 20pt, weight: "bold", style: "normal", fill: rgb("#f37021"))[#(val2.split("\n").join(linebreak()))]))) 
  ]
  // @item kind=1 x=174.600 y=147.900 w=80.000 h=10.000 zOrder=0 imagePath= text=%24%28competitorName%29 fontFamily=Space%20Grotesk fontSize=20 bold=1 italic=0 color=%23f37021 halign=4 scaleProportional=1
  #let val3 = str(page.at("competitorName", default: ""))
  #if val3 != "" [
    #place(dx: 174.600mm, dy: 147.900mm, box(width: 80.000mm, height: 10.000mm, align(center + horizon, text(font: "Space Grotesk", size: 20pt, weight: "bold", style: "normal", fill: rgb("#f37021"))[#(val3.split("\n").join(linebreak()))]))) 
  ]
  #if i < pages.len() - 1 [#pagebreak()]
]
