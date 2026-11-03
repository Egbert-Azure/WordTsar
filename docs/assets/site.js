// Draws the WordStar ruler to the exact width of the writing column,
// and shows the latest release number from GitHub.
(function () {
  var ruler = document.querySelector("[data-ruler]");

  function drawRuler() {
    if (!ruler) return;
    var probe = document.createElement("span");
    probe.textContent = "0000000000";
    probe.style.position = "absolute";
    probe.style.visibility = "hidden";
    ruler.appendChild(probe);
    var charWidth = probe.getBoundingClientRect().width / 10;
    ruler.removeChild(probe);
    if (!charWidth) return;

    var columns = Math.max(12, Math.floor(ruler.clientWidth / charWidth));
    var line = "";
    for (var i = 1; i <= columns; i++) {
      if (i === 1) line += "L";
      else if (i === columns) line += "R";
      else line += (i - 1) % 5 === 0 ? "!" : "-";
    }
    ruler.textContent = line;
  }

  drawRuler();
  if (document.fonts && document.fonts.ready) document.fonts.ready.then(drawRuler);
  var timer;
  window.addEventListener("resize", function () {
    clearTimeout(timer);
    timer = setTimeout(drawRuler, 120);
  });

  var version = document.querySelector("[data-version]");
  if (version && window.fetch) {
    fetch("https://api.github.com/repos/Egbert-Azure/WordTsar/releases/latest")
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (release) {
        if (!release || !release.tag_name) return;
        version.textContent = "Latest version: " + release.tag_name.replace(/^v/, "") + ".";
        version.hidden = false;
      })
      .catch(function () {});
  }
})();
