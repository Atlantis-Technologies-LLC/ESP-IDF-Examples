document.addEventListener("DOMContentLoaded", function () {
    // Fetch and display the current firmware version
    fetch('/version')
        .then(response => response.json())
        .then(data => {
            // Update version
            document.getElementById('current-version').textContent = data.version;
            document.getElementById('version-number').textContent = data.version;
        })
        .catch(error => console.error('Error fetching data:', error));

    // File input change handler
    document.getElementById('firmware').addEventListener('change', function (e) {
        const file = e.target.files[0];
        const details = document.getElementById('file-details');

        if (file) {
            details.textContent = `Selected file: ${file.name} (${formatBytes(file.size)})`;

            if (!file.name.endsWith('.pkg')) {
                details.textContent += " Invalid file type. Please select a .pkg file.";
                document.getElementById('update-btn').disabled = true;
            } else {
                document.getElementById('update-btn').disabled = false;
            }
        }
    });

    // Form submit handler
    document.getElementById('upload-form').addEventListener('submit', async function (e) {
        e.preventDefault();

        const fileInput = document.getElementById('firmware');
        const updateButton = document.getElementById('update-btn');
        const statusCard = document.getElementById('status-card'); 
        const status = document.getElementById('status');
        const progressCard = document.getElementById('progress-card');  // ✅ Fix: Show entire card
        const progressContainer = document.getElementById('progress-container');
        const progressBar = document.getElementById('progress');
        const progressText = document.getElementById('progress-text');
        const file = fileInput.files[0];

        if (!file) {
            status.textContent = 'Please select a file';
            status.className = 'error';
            return;
        }

        updateButton.disabled = true;
        progressCard.style.display = 'block';  // ✅ Fix: Show entire progress card
        progressContainer.style.display = 'block';  // ✅ Ensure container is visible
        progressBar.style.width = '0%';  // ✅ Reset progress bar
        progressText.textContent = `0% (0 Bytes / ${formatBytes(file.size)})`;  // ✅ Reset text
        statusCard.style.display = 'block';
        status.textContent = 'Uploading...';
        status.className = 'status-info';

        try {
            const xhr = new XMLHttpRequest();
            
            xhr.upload.onprogress = function (e) {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    
                    if (progressBar) {  // ✅ Prevent errors
                        progressBar.style.width = percentComplete + '%';
                        progressText.textContent = `${Math.round(percentComplete)}% (${formatBytes(e.loaded)} / ${formatBytes(e.total)})`;
                    }
                }
            };

            // Wrap XHR in a promise
            const uploadPromise = new Promise((resolve, reject) => {
                xhr.onload = () => {
                    if (xhr.status >= 200 && xhr.status < 300) {
                        resolve(xhr.responseText);
                    } else {
                        reject(new Error(xhr.responseText || `Upload failed with status ${xhr.status}`));
                    }
                };
                xhr.onerror = () => reject(new Error('Upload failed'));
            });

            xhr.open('POST', '/update_firmware', true);
            xhr.setRequestHeader('Content-Type', 'application/x-pkg');
            xhr.send(file);

            const result = await uploadPromise;
            status.textContent = result;
            status.className = 'status-success';
            progressText.textContent = 'Upload complete!';

            // Device will reboot, so we'll show a countdown
            let countdown = 10;
            const updateInterval = setInterval(() => {
                countdown--;
                if (countdown <= 0) {
                    clearInterval(updateInterval);
                    window.location.reload();
                } else {
                    status.textContent = `${result} Reloading page in ${countdown} seconds...`;
                }
            }, 1000);

        } catch (error) {
            console.error('Upload error:', error);
            status.textContent = error.message || 'Upload failed';
            status.className = 'status-error';
            progressBar.style.width = '0%';
            progressText.textContent = '';
        } finally {
            updateButton.disabled = false;
        }
    });

    function formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }
});