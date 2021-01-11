/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Contains implementation of a class VirtualCamera that encapsulates
 * functionality common to all version 2.0 virtual camera devices.  Instances
 * of this class (for each virtual camera) are created during the construction
 * of the VirtualCameraFactory instance.  This class serves as an entry point
 * for all camera API calls that defined by camera2_device_ops_t API.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "VirtualCamera2_Camera"
#include <log/log.h>

#include "VirtualCamera2.h"
#include "system/camera_metadata.h"

namespace android {

/* Constructs VirtualCamera2 instance.
 * Param:
 *  cameraId - Zero based camera identifier, which is an index of the camera
 *      instance in camera factory's array.
 *  module - Virtual camera HAL module descriptor.
 */
VirtualCamera2::VirtualCamera2(int cameraId, struct hw_module_t *module)
    : VirtualBaseCamera(cameraId, CAMERA_DEVICE_API_VERSION_2_0, &common, module) {
    common.close = VirtualCamera2::close;
    ops = &sDeviceOps;
    priv = this;

    mNotifyCb = NULL;

    mRequestQueueSrc = NULL;
    mFrameQueueDst = NULL;

    mVendorTagOps.get_camera_vendor_section_name = VirtualCamera2::get_camera_vendor_section_name;
    mVendorTagOps.get_camera_vendor_tag_name = VirtualCamera2::get_camera_vendor_tag_name;
    mVendorTagOps.get_camera_vendor_tag_type = VirtualCamera2::get_camera_vendor_tag_type;
    mVendorTagOps.parent = this;

    mStatusPresent = true;
}

/* Destructs VirtualCamera2 instance. */
VirtualCamera2::~VirtualCamera2() {}

/****************************************************************************
 * Abstract API
 ***************************************************************************/

/****************************************************************************
 * Public API
 ***************************************************************************/

status_t VirtualCamera2::Initialize(const char *device_name, const char *frame_dims,
                                    const char *facing_dir) {
    return NO_ERROR;
}

/****************************************************************************
 * Camera API implementation
 ***************************************************************************/

status_t VirtualCamera2::connectCamera(hw_device_t **device) {
    *device = &common;
    return NO_ERROR;
}

status_t VirtualCamera2::closeCamera() { return NO_ERROR; }

status_t VirtualCamera2::getCameraInfo(struct camera_info *info) {
    return VirtualBaseCamera::getCameraInfo(info);
}

/****************************************************************************
 * Camera Device API implementation.
 * These methods are called from the camera API callback routines.
 ***************************************************************************/

/** Request input queue */

int VirtualCamera2::requestQueueNotify() { return INVALID_OPERATION; }

/** Count of requests in flight */
int VirtualCamera2::getInProgressCount() { return INVALID_OPERATION; }

/** Cancel all captures in flight */
int VirtualCamera2::flushCapturesInProgress() { return INVALID_OPERATION; }

/** Construct a default request for a given use case */
int VirtualCamera2::constructDefaultRequest(int request_template, camera_metadata_t **request) {
    return INVALID_OPERATION;
}

/** Output stream creation and management */

int VirtualCamera2::allocateStream(uint32_t width, uint32_t height, int format,
                                   const camera2_stream_ops_t *stream_ops, uint32_t *stream_id,
                                   uint32_t *format_actual, uint32_t *usage,
                                   uint32_t *max_buffers) {
    return INVALID_OPERATION;
}

int VirtualCamera2::registerStreamBuffers(uint32_t stream_id, int num_buffers,
                                          buffer_handle_t *buffers) {
    return INVALID_OPERATION;
}

int VirtualCamera2::releaseStream(uint32_t stream_id) { return INVALID_OPERATION; }

/** Reprocessing input stream management */

int VirtualCamera2::allocateReprocessStream(uint32_t width, uint32_t height, uint32_t format,
                                            const camera2_stream_in_ops_t *reprocess_stream_ops,
                                            uint32_t *stream_id, uint32_t *consumer_usage,
                                            uint32_t *max_buffers) {
    return INVALID_OPERATION;
}

int VirtualCamera2::allocateReprocessStreamFromStream(
    uint32_t output_stream_id, const camera2_stream_in_ops_t *reprocess_stream_ops,
    uint32_t *stream_id) {
    return INVALID_OPERATION;
}

int VirtualCamera2::releaseReprocessStream(uint32_t stream_id) { return INVALID_OPERATION; }

/** 3A triggering */

int VirtualCamera2::triggerAction(uint32_t trigger_id, int ext1, int ext2) {
    return INVALID_OPERATION;
}

/** Custom tag query methods */

const char *VirtualCamera2::getVendorSectionName(uint32_t tag) { return NULL; }

const char *VirtualCamera2::getVendorTagName(uint32_t tag) { return NULL; }

int VirtualCamera2::getVendorTagType(uint32_t tag) { return -1; }

/** Debug methods */

int VirtualCamera2::dump(int fd) { return INVALID_OPERATION; }

/****************************************************************************
 * Private API.
 ***************************************************************************/

/****************************************************************************
 * Camera API callbacks as defined by camera2_device_ops structure.  See
 * hardware/libhardware/include/hardware/camera2.h for information on each
 * of these callbacks. Implemented in this class, these callbacks simply
 * dispatch the call into an instance of VirtualCamera2 class defined by the
 * 'camera_device2' parameter, or set a member value in the same.
 ***************************************************************************/

VirtualCamera2 *getInstance(const camera2_device_t *d) {
    const VirtualCamera2 *cec = static_cast<const VirtualCamera2 *>(d);
    return const_cast<VirtualCamera2 *>(cec);
}

int VirtualCamera2::set_request_queue_src_ops(const camera2_device_t *d,
                                              const camera2_request_queue_src_ops *queue_src_ops) {
    VirtualCamera2 *ec = getInstance(d);
    ec->mRequestQueueSrc = queue_src_ops;
    return NO_ERROR;
}

int VirtualCamera2::notify_request_queue_not_empty(const camera2_device_t *d) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->requestQueueNotify();
}

int VirtualCamera2::set_frame_queue_dst_ops(const camera2_device_t *d,
                                            const camera2_frame_queue_dst_ops *queue_dst_ops) {
    VirtualCamera2 *ec = getInstance(d);
    ec->mFrameQueueDst = queue_dst_ops;
    return NO_ERROR;
}

int VirtualCamera2::get_in_progress_count(const camera2_device_t *d) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->getInProgressCount();
}

int VirtualCamera2::flush_captures_in_progress(const camera2_device_t *d) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->flushCapturesInProgress();
}

int VirtualCamera2::construct_default_request(const camera2_device_t *d, int request_template,
                                              camera_metadata_t **request) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->constructDefaultRequest(request_template, request);
}

int VirtualCamera2::allocate_stream(const camera2_device_t *d, uint32_t width, uint32_t height,
                                    int format, const camera2_stream_ops_t *stream_ops,
                                    uint32_t *stream_id, uint32_t *format_actual, uint32_t *usage,
                                    uint32_t *max_buffers) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->allocateStream(width, height, format, stream_ops, stream_id, format_actual, usage,
                              max_buffers);
}

int VirtualCamera2::register_stream_buffers(const camera2_device_t *d, uint32_t stream_id,
                                            int num_buffers, buffer_handle_t *buffers) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->registerStreamBuffers(stream_id, num_buffers, buffers);
}
int VirtualCamera2::release_stream(const camera2_device_t *d, uint32_t stream_id) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->releaseStream(stream_id);
}

int VirtualCamera2::allocate_reprocess_stream(const camera2_device_t *d, uint32_t width,
                                              uint32_t height, uint32_t format,
                                              const camera2_stream_in_ops_t *reprocess_stream_ops,
                                              uint32_t *stream_id, uint32_t *consumer_usage,
                                              uint32_t *max_buffers) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->allocateReprocessStream(width, height, format, reprocess_stream_ops, stream_id,
                                       consumer_usage, max_buffers);
}

int VirtualCamera2::allocate_reprocess_stream_from_stream(
    const camera2_device_t *d, uint32_t output_stream_id,
    const camera2_stream_in_ops_t *reprocess_stream_ops, uint32_t *stream_id) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->allocateReprocessStreamFromStream(output_stream_id, reprocess_stream_ops, stream_id);
}

int VirtualCamera2::release_reprocess_stream(const camera2_device_t *d, uint32_t stream_id) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->releaseReprocessStream(stream_id);
}

int VirtualCamera2::trigger_action(const camera2_device_t *d, uint32_t trigger_id, int ext1,
                                   int ext2) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->triggerAction(trigger_id, ext1, ext2);
}

int VirtualCamera2::set_notify_callback(const camera2_device_t *d,
                                        camera2_notify_callback notify_cb, void *user) {
    VirtualCamera2 *ec = getInstance(d);
    Mutex::Autolock l(ec->mMutex);
    ec->mNotifyCb = notify_cb;
    ec->mNotifyUserPtr = user;
    return NO_ERROR;
}

int VirtualCamera2::get_metadata_vendor_tag_ops(const camera2_device_t *d,
                                                vendor_tag_query_ops_t **ops) {
    VirtualCamera2 *ec = getInstance(d);
    *ops = static_cast<vendor_tag_query_ops_t *>(&ec->mVendorTagOps);
    return NO_ERROR;
}

const char *VirtualCamera2::get_camera_vendor_section_name(const vendor_tag_query_ops_t *v,
                                                           uint32_t tag) {
    VirtualCamera2 *ec = static_cast<const TagOps *>(v)->parent;
    return ec->getVendorSectionName(tag);
}

const char *VirtualCamera2::get_camera_vendor_tag_name(const vendor_tag_query_ops_t *v,
                                                       uint32_t tag) {
    VirtualCamera2 *ec = static_cast<const TagOps *>(v)->parent;
    return ec->getVendorTagName(tag);
}

int VirtualCamera2::get_camera_vendor_tag_type(const vendor_tag_query_ops_t *v, uint32_t tag) {
    VirtualCamera2 *ec = static_cast<const TagOps *>(v)->parent;
    return ec->getVendorTagType(tag);
}

int VirtualCamera2::dump(const camera2_device_t *d, int fd) {
    VirtualCamera2 *ec = getInstance(d);
    return ec->dump(fd);
}

int VirtualCamera2::close(struct hw_device_t *device) {
    VirtualCamera2 *ec =
        static_cast<VirtualCamera2 *>(reinterpret_cast<camera2_device_t *>(device));
    if (ec == NULL) {
        ALOGE("%s: Unexpected NULL camera2 device", __FUNCTION__);
        return -EINVAL;
    }
    return ec->closeCamera();
}

void VirtualCamera2::sendNotification(int32_t msgType, int32_t ext1, int32_t ext2, int32_t ext3) {
    camera2_notify_callback notifyCb;
    {
        Mutex::Autolock l(mMutex);
        notifyCb = mNotifyCb;
    }
    if (notifyCb != NULL) {
        notifyCb(msgType, ext1, ext2, ext3, mNotifyUserPtr);
    }
}

camera2_device_ops_t VirtualCamera2::sDeviceOps = {
    VirtualCamera2::set_request_queue_src_ops,
    VirtualCamera2::notify_request_queue_not_empty,
    VirtualCamera2::set_frame_queue_dst_ops,
    VirtualCamera2::get_in_progress_count,
    VirtualCamera2::flush_captures_in_progress,
    VirtualCamera2::construct_default_request,
    VirtualCamera2::allocate_stream,
    VirtualCamera2::register_stream_buffers,
    VirtualCamera2::release_stream,
    VirtualCamera2::allocate_reprocess_stream,
    VirtualCamera2::allocate_reprocess_stream_from_stream,
    VirtualCamera2::release_reprocess_stream,
    VirtualCamera2::trigger_action,
    VirtualCamera2::set_notify_callback,
    VirtualCamera2::get_metadata_vendor_tag_ops,
    VirtualCamera2::dump};

}; /* namespace android */
