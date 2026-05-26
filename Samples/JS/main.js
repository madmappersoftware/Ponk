
import ponkManager from "./PonkManager.js";

function main() {
  console.log("NODEJS VERSION ", process.version);
  console.info('-------------------\n\nPonk Js\n\n\-------------------');
  ponkManager.setServerName("Ponk JS");
  ponkManager.connect();
}
main()


setInterval(function () {
  let frame = []
  let iMax = 10
  for (let i = 0; i < iMax; i++) {
    let cx = Math.sin(2 * Math.PI * i / iMax) / 2
    let cy = Math.cos(2 * Math.PI * i / iMax) / 2
    let r = Math.sin(2 * Math.PI * Date.now() / 5000) / 20
    let p1 = [cx - r, cy + r]
    let p2 = [cx, cy - r]
    let p3 = [cx + r, cy + r]
    let red = Math.abs(Math.sin(2 * Math.PI * i / iMax)) * 255
    let green = Math.abs(Math.cos(2 * Math.PI * i / iMax)) * 255
    let blue = Math.abs(Math.sin(2 * Math.PI * Date.now() / 5000)) * 255
    ponkManager.setPath(frame, i, [p1, p2, p3, p1], red, green, blue)
  }
  ponkManager.sendFrame(frame, "PONKJS", 42)
}, 16)

setInterval(function () {
  let frame = []
  let iMax = 3
  for (let i = 0; i < iMax; i++) {
    let cx = Math.sin(2 * Math.PI * Date.now() / 5000 + 2 * Math.PI * i / iMax) / 2
    let cy = Math.cos(2 * Math.PI * Date.now() / 5000 + 2 * Math.PI * i / iMax) / 2
    ponkManager.circle(frame, i, cx, cy,0.5 / 2)
  }
  ponkManager.sendFrame(frame, "circle", 2)
}, 16)
