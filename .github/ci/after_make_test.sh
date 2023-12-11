# Package 
make package
mkdir artifacts && mv *.deb artifacts/

# Deploy to nexus
NEXUS_REPO="ppa-public"
NEXUS_ENDPOINT="artifacts.aws.cloud.mov.ai"

for file in artifacts/*.deb
do
    RETURN_CODE=$(curl -u "$nexus_publisher_user:$nexus_publisher_password" \
    -H "Content-Type: multipart/form-data" \
    --data-binary "@$file" \
    -w '%{http_code}' \
    "https://$NEXUS_ENDPOINT/repository/$NEXUS_REPO/")

    #retry
    if [[ ! "$RETURN_CODE" =~ ^(200|201|202)$ ]]; then
    echo "Failed upload with $RETURN_CODE. Retrying"

    RETURN_CODE=$(curl -u "$nexus_publisher_user:$nexus_publisher_password" \
        -H "Content-Type: multipart/form-data" \
        --data-binary "@$file" \
        -w '%{http_code}' \
        "https://$NEXUS_ENDPOINT/repository/$NEXUS_REPO/")
    fi

    if [[ ! "$RETURN_CODE" =~ ^(200|201|202)$ ]]; then
    echo "Failed upload with $RETURN_CODE. Exiting"
    exit 1
    fi

done