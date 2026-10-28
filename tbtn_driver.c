#include <linux/module.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>

static const struct acpi_device_id tbtn_device_ids[] = {
    {"MAT002A", 0}, // FZ-G1 (0x2A003434)
    {"MAT002B", 0}, // FZ-G1 (0x2B003434)
    {"MAT0034", 0}, // CF-20 (TBTN)
    {"", 0},
};

MODULE_DEVICE_TABLE(acpi, tbtn_device_ids);

struct tbtn_dev
{
    acpi_handle handle;
    struct input_dev *input_dev;
};

static const struct key_entry tbtn_keymap[] = {
    // Rotation lock button: HINF 0x36 (press) / 0x37 (release) -> KEY_F13
    {KE_KEY, 55, {KEY_F13}},
    // A1 button: HINF 0x38 (press) / 0x39 (release) -> KEY_F14
    {KE_KEY, 57, {KEY_F14}},
    // A2 button: HINF 0x42 (press) / 0x43 (release) -> KEY_F15
    {KE_KEY, 67, {KEY_F15}},
    {KE_END, 0}};

// ACPI notification handler
static void tbtn_notify_handler(struct acpi_device *device, u32 event)
{
    struct tbtn_dev *tbtn = acpi_driver_data(device);
    unsigned long long hinf_result;
    acpi_status status;
    unsigned int key_value;      // Key code extracted from HINF (bits 0-6)
    unsigned int key_event_type; // Event state from HINF (bit 7: 0x80 = press, 0x00 = release)

    if (!tbtn || !tbtn->input_dev)
    {
        pr_warn("tbtn: Received notify for uninitialized device\n");
        return;
    }

    switch (event)
    {
    case 0x80: // Notification event from ACPI TBTN device
        pr_info("tbtn: Notify 0x80 received\n");

        // Evaluate HINF method to retrieve button event info
        status = acpi_evaluate_integer(tbtn->handle, "HINF", NULL, &hinf_result);
        if (ACPI_FAILURE(status))
        {
            pr_err("tbtn: Failed to evaluate HINF: %s\n", acpi_format_exception(status));
            return;
        }

        pr_info("tbtn: HINF returned 0x%llx\n", hinf_result);

        key_value = (unsigned int)(hinf_result & 0x7F);
        key_event_type = (unsigned int)(hinf_result & 0x80);

        unsigned int report_key_value = 0;
        int event_type_to_report = -1; // 1 = press, 0 = release, -1 = unhandled

        switch (key_value)
        {
        case 54:                      // Rotation lock button press (HINF 0x36)
            report_key_value = 55;
            event_type_to_report = 1; // Press
            break;
        case 55:                      // Rotation lock button release (HINF 0x37)
            report_key_value = 55;
            event_type_to_report = 0; // Release
            break;
        case 56:                      // A1 button press (HINF 0x38)
            report_key_value = 57;
            event_type_to_report = 1; // Press
            break;
        case 57:                      // A1 button release (HINF 0x39)
            report_key_value = 57;
            event_type_to_report = 0; // Release
            break;
        case 66:                      // A2 button press (HINF 0x42)
            report_key_value = 67;
            event_type_to_report = 1; // Press
            break;
        case 67:                      // A2 button release (HINF 0x43)
            report_key_value = 67;
            event_type_to_report = 0; // Release
            break;
        default:
            pr_warn("tbtn: Received unhandled HINF key_value: 0x%x (raw=0x%llx)\n", key_value, hinf_result);
            return;
        }

        // Report key event via sparse_keymap with autorelease=false to support button holds and chords
        if (event_type_to_report != -1)
        {
            if (!sparse_keymap_report_event(tbtn->input_dev, report_key_value, event_type_to_report, false))
            {
                pr_warn("tbtn: Failed to report HINF event via sparse_keymap: HINF_raw=0x%llx, report_key=%u, report_type=%d\n",
                        hinf_result, report_key_value, event_type_to_report);
            }
            else
            {
                pr_info("tbtn: Reported key event: report_key=%u, report_type=%d\n",
                        report_key_value, event_type_to_report);
            }
        }
        break;
    default:
        pr_info("tbtn: Received unknown event 0x%x\n", event);
        break;
    }
}

// Device add callback
static int tbtn_add(struct acpi_device *device)
{
    struct tbtn_dev *tbtn;
    struct input_dev *input_dev;
    int error;

    pr_info("tbtn: Device add called for %s\n", acpi_device_hid(device));

    tbtn = devm_kzalloc(&device->dev, sizeof(struct tbtn_dev), GFP_KERNEL);
    if (!tbtn)
    {
        return -ENOMEM;
    }

    tbtn->handle = device->handle;
    device->driver_data = tbtn;

    // Allocate and configure input device
    input_dev = devm_input_allocate_device(&device->dev);
    if (!input_dev)
    {
        pr_err("tbtn: Failed to allocate input device\n");
        return -ENOMEM;
    }

    input_dev->name = "Panasonic TBTN Tablet Buttons";
    input_dev->phys = "tbtn/input0";
    input_dev->id.bustype = BUS_HOST;

    // Set up sparse keymap
    error = sparse_keymap_setup(input_dev, tbtn_keymap, NULL);
    if (error)
    {
        pr_err("tbtn: Failed to setup keymap: %d\n", error);
        return error;
    }

    tbtn->input_dev = input_dev;

    // Register input device
    error = input_register_device(tbtn->input_dev);
    if (error)
    {
        pr_err("tbtn: Failed to register input device: %d\n", error);
        return error;
    }

    pr_info("tbtn: Input device registered for %s\n", acpi_device_hid(device));
    return 0;
}

// Device remove callback
static void tbtn_remove(struct acpi_device *device)
{
    pr_info("tbtn: Device remove called for %s\n", acpi_device_hid(device));
}

// ACPI driver structure
static struct acpi_driver tbtn_acpi_driver = {
    .name = "tbtn_driver",
    .class = "tbtn",
    .ids = tbtn_device_ids,
    .ops = {
        .add = tbtn_add,
        .remove = tbtn_remove,
        .notify = tbtn_notify_handler,
    },
};

// Module initialization
static int __init tbtn_init(void)
{
    int result;
    result = acpi_bus_register_driver(&tbtn_acpi_driver);
    if (result < 0)
    {
        pr_err("tbtn: Error registering ACPI driver\n");
        return result;
    }
    pr_info("tbtn: ACPI driver registered\n");
    return 0;
}

// Module exit
static void __exit tbtn_exit(void)
{
    acpi_bus_unregister_driver(&tbtn_acpi_driver);
    pr_info("tbtn: ACPI driver unregistered\n");
}

module_init(tbtn_init);
module_exit(tbtn_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sh1ma & kenhou");
MODULE_DESCRIPTION("Panasonic TOUGHPAD/TOUGHBOOK ACPI TBTN Tablet Button Driver");