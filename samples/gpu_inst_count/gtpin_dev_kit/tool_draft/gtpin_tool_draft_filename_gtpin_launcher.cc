
#include <fstream>

#include "gtpin_tool_draft_filename.hpp"

using namespace gtpin_prof;
using namespace gtpin;

/* =============================================================================================
 */
// GTPin loader part, functions that make it possible to use with GTPin loader
/* =============================================================================================
 */

class GtpinToolDraftTxtWriter : public DefaultTxtGTPinWriter {
 public:
  using DefaultTxtGTPinWriter::DefaultTxtGTPinWriter;

  bool Init() final {
    if (sh == nullptr) {
      sh = new StreamHolder(std::cout);
    }
    if (sh == nullptr) return false;
    return true;
  }
  void WriteToolProfilerData(const std::shared_ptr<ProfilerData> profData) final {
    auto gtpinToolDraftProfRes = std::dynamic_pointer_cast<GtpinToolDraftProfilerData>(profData);
    PTI_ASSERT(gtpinToolDraftProfRes != nullptr && "Error during data write");
  }
  void WriteToolKernelData(const std::shared_ptr<ProfilerData> profData,
                           const std::shared_ptr<KernelData> kerData) final {
    auto gtpinToolDraftProfRes = std::dynamic_pointer_cast<GtpinToolDraftProfilerData>(profData);
    auto gtpinToolDraftKernRes = std::dynamic_pointer_cast<GtpinToolDraftKernelData>(kerData);
    PTI_ASSERT(gtpinToolDraftProfRes != nullptr && "Error during data write");
    PTI_ASSERT(gtpinToolDraftKernRes != nullptr && "Error during data write");
  }
  void WriteToolInvocationData(const std::shared_ptr<ProfilerData> profData,
                               const std::shared_ptr<KernelData> kerData,
                               const std::shared_ptr<InvocationData> invData) final {
    auto gtpinToolDraftProfRes = std::dynamic_pointer_cast<GtpinToolDraftProfilerData>(profData);
    auto gtpinToolDraftKernRes = std::dynamic_pointer_cast<GtpinToolDraftKernelData>(kerData);
    auto gtpinToolDraftInvRes = std::dynamic_pointer_cast<GtpinToolDraftInvocationData>(invData);
    PTI_ASSERT(gtpinToolDraftProfRes != nullptr && "Error during data write");
    PTI_ASSERT(gtpinToolDraftKernRes != nullptr && "Error during data write");
    PTI_ASSERT(gtpinToolDraftInvRes != nullptr && "Error during data write");
  }
  void WriteToolResultData(const std::shared_ptr<ProfilerData> profData,
                           const std::shared_ptr<KernelData> kerData,
                           const std::shared_ptr<InvocationData> invData,
                           const std::shared_ptr<ResultData> resData) final {
    auto gtpinToolDraftProfRes = std::dynamic_pointer_cast<GtpinToolDraftProfilerData>(profData);
    auto gtpinToolDraftKernRes = std::dynamic_pointer_cast<GtpinToolDraftKernelData>(kerData);
    auto gtpinToolDraftInvRes = std::dynamic_pointer_cast<GtpinToolDraftInvocationData>(invData);
    auto gtpinToolDraftRes = std::dynamic_pointer_cast<GtpinToolDraftResultData>(resData);
    PTI_ASSERT(gtpinToolDraftProfRes != nullptr && "Error during data write");
    PTI_ASSERT(gtpinToolDraftKernRes != nullptr && "Error during data write");
    PTI_ASSERT(gtpinToolDraftInvRes != nullptr && "Error during data write");
    PTI_ASSERT(gtpinToolDraftRes != nullptr && "Error during data write");
  }
};

class GtpinToolDraftJsonWriter : public DefaultJsonGTPinWriter {
 public:
  using DefaultJsonGTPinWriter::DefaultJsonGTPinWriter;

  bool Init() final {
    if (sh != nullptr) {
      delete sh;
    }
    std::string jsonPath =
        std::string(GTPin_GetCore()->ProfileDir()) + DEL + "gtpin_tool_draft_results.json";
    m_jsonFile = std::ofstream(jsonPath);
    sh = new StreamHolder(m_jsonFile);
    if (sh == nullptr) return false;
    return true;
  }

  void WriteToolProfilerData(const std::shared_ptr<ProfilerData> profData) final {
    auto gtpinToolDraftProfRes = std::dynamic_pointer_cast<GtpinToolDraftProfilerData>(profData);
    PTI_ASSERT(gtpinToolDraftProfRes != nullptr && "Error during data write");
  }
  void WriteToolKernelData(const std::shared_ptr<ProfilerData> profData,
                           const std::shared_ptr<KernelData> kerData) final {
    auto gtpinToolDraftProfRes = std::dynamic_pointer_cast<GtpinToolDraftProfilerData>(profData);
    auto gtpinToolDraftKernRes = std::dynamic_pointer_cast<GtpinToolDraftKernelData>(kerData);
    PTI_ASSERT(gtpinToolDraftKernRes != nullptr && "Error during data write");
  }
  void WriteToolInvocationData(const std::shared_ptr<ProfilerData> profData,
                               const std::shared_ptr<KernelData> kerData,
                               const std::shared_ptr<InvocationData> invData) final {
    auto gtpinToolDraftProfRes = std::dynamic_pointer_cast<GtpinToolDraftProfilerData>(profData);
    auto gtpinToolDraftKernRes = std::dynamic_pointer_cast<GtpinToolDraftKernelData>(kerData);
    auto gtpinToolDraftInvRes = std::dynamic_pointer_cast<GtpinToolDraftInvocationData>(invData);
    PTI_ASSERT(gtpinToolDraftInvRes != nullptr && "Error during data write");
  }
  void WriteToolResultData(const std::shared_ptr<ProfilerData> profData,
                           const std::shared_ptr<KernelData> kerData,
                           const std::shared_ptr<InvocationData> invData,
                           const std::shared_ptr<ResultData> resData) final {
    auto gtpinToolDraftProfRes = std::dynamic_pointer_cast<GtpinToolDraftProfilerData>(profData);
    auto gtpinToolDraftKernRes = std::dynamic_pointer_cast<GtpinToolDraftKernelData>(kerData);
    auto gtpinToolDraftInvRes = std::dynamic_pointer_cast<GtpinToolDraftInvocationData>(invData);
    auto gtpinToolDraftRes = std::dynamic_pointer_cast<GtpinToolDraftResultData>(resData);
    PTI_ASSERT(gtpinToolDraftRes != nullptr && "Error during data write");
  }

 private:
  std::ofstream m_jsonFile;
};

GtpinToolDraftProfiler* toolHandle = nullptr;

std::shared_ptr<GtpinToolDraftTxtWriter> txtWriter =
    std::make_shared<GtpinToolDraftTxtWriter>(std::cout);
std::shared_ptr<GtpinToolDraftJsonWriter> jsonWriter = std::make_shared<GtpinToolDraftJsonWriter>();

std::shared_ptr<MultipleGTPinWriter> multWriter = std::make_shared<MultipleGTPinWriter>(
    std::vector<std::shared_ptr<GTPinDataWriterBase>>{txtWriter, jsonWriter});

void OnFini() {
  if (toolHandle != nullptr) {
    toolHandle->Stop();
    delete toolHandle;
    toolHandle = nullptr;
  }
}

EXPORT_C_FUNC void GTPin_Entry(int argc, const char* argv[]) {
  ConfigureGTPin(argc, argv);
  atexit(OnFini);

  toolHandle = new GtpinToolDraftProfiler(multWriter);
  auto status = toolHandle->Start();

  if (PROF_STATUS_SUCCESS != status) {
    std::cout << GTPIN_LAST_ERROR_STR << std::endl;
  }
}
