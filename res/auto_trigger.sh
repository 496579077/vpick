#!/bin/sh

never_trigger_before=false
check_if_trigger_before() {
    local manufacturer=$(getprop ro.product.manufacturer)
    local brand=$(getprop ro.product.brand)
    local model=$(getprop ro.product.model)
    local board=$(getprop ro.product.board)
    local product=$(getprop ro.build.product)

    [ "$manufacturer" = "rockchip" ] && never_trigger_before=true
    [ "$brand" = "rockchip" ] && never_trigger_before=true
    [ "$model" = "rk3588_docker" ] && never_trigger_before=true
    [ "$board" = "rk30sdk" ] && never_trigger_before=true
    [ "$product" = "rk3588_docker" ] && never_trigger_before=true
}

maybe_trigger_vpck() {
    if [ "$never_trigger_before" = "true" ]; then
        # Check if vpick command exists
        if ! command -v vpick &>/dev/null; then
            echo "Error: vpick command not found!"
            return 1
        fi

        # Get the count of available_cnt items
        available_cnt=$(vpick -l | wc -l || echo 0)
        
        # Subtract 4 if avalibal_cnt > 4
        if ((available_cnt > 4)); then
            available_cnt=$((available_cnt - 4))
        fi

        if ((available_cnt <= 0)); then
            echo "no avalibal backup"
            return 0;
        fi
        random_index=$((RANDOM % available_cnt + 1))
        echo "trigger vpick restore ... 1/2"
        vpick -r --index $random_index --setprop-cmd gifsetprop
        echo "trigger vpick restore ... 2/2"
        sleep 1
        #vpick -r --index $random_index
        nohup vpick -r --index $random_index > /dev/null 2>&1 &
        echo "trigger vpick finished"
        local brand=$(getprop ro.product.brand)
        local model=$(getprop ro.product.model)
        local version=$(getprop ro.build.version.release)
        
        echo "****************************************************"
        echo "new device info:(index=$random_index)"
        echo "brand  : $brand"
        echo "model  : $model"
        echo "version: Android $version"
        echo "****************************************************"
    fi
}

check_if_trigger_before
maybe_trigger_vpck
