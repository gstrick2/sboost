.. _show_index_settings_syntax:

SHOW INDEX SETTINGS syntax
--------------------------

.. code-block:: mysql


    SHOW INDEX index_name[.N | CHUNK N] SETTINGS

Displays per-index settings in a ``manticore.conf`` compliant file format,
similar to the :ref:`–dumpconfig <indextool_command_reference>`
option of the indextool. The report provides a breakdown of all the
index settings, including tokenizer and dictionary options. You may also
specify a particular :ref:`chunk
number <rt_mem_limit>` for the RT
indexes.
