//waits for HTML to fully parse before running code
window.addEventListener("DOMContentLoaded", () => {
  //grab <img> element that displays the image frame
  const imgElement = document.getElementById("video-stream");
  //grabs <span> element that shows switch state
  const switchStateElement = document.getElementById("switch-state");

  //function to update the image
  function updateImage() {
    imgElement.src = `http://10.245.161.32:3000/output.png?ts=${Date.now()}`;
  }

  //function to fetch the latest switch state from the server
  function updateSwitchState() {
    //urly for switch state endpoint
    const url = `http://10.245.161.32:3000/switchState`;
    //perform HTTP GET
    fetch(url)
      .then(response => response.json())
      .then(data => {
        console.log("Fetched switch state:", data.switchState);
        //update switch state or be "Unknown"
        switchStateElement.textContent = data.switchState || "Unknown";
      })
      .catch(err => console.error("Error fetching switch state:", err));
  }
  
  //refresh the image and switch state every 1.5 seconds
  setInterval(() => {
    updateImage();
    updateSwitchState();
  }, 1500);
});

//back to overview page button
const backButton = document.getElementById('back-button'); //refrence button
backButton.addEventListener('click', () => {
  window.location.href = 'overview_page.html'; //redirects to overview page when clicked
});

//back to map page button
const backToMapButton = document.getElementById('back-to-map-button'); //refrence button
backToMapButton.addEventListener('click', () => {
  window.location.href = 'map_builder.html';//redirect to map builder page when clicked
});